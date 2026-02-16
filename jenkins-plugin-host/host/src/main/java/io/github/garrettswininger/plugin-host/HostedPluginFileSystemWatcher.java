package io.github.garrettswininger.pluginhost;

import static java.nio.file.StandardWatchEventKinds.ENTRY_CREATE;
import static java.nio.file.StandardWatchEventKinds.ENTRY_DELETE;
import static java.nio.file.StandardWatchEventKinds.ENTRY_MODIFY;
import static java.nio.file.StandardWatchEventKinds.OVERFLOW;

import hudson.Extension;
import hudson.model.RootAction;
import java.io.File;
import java.io.IOException;
import java.nio.file.FileSystems;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.LocalDateTime;
import java.time.temporal.ChronoUnit;
import java.util.List;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.DelayQueue;
import java.util.concurrent.Delayed;
import java.util.concurrent.TimeUnit;
import java.util.logging.Logger;
import java.util.stream.Stream;
import jenkins.model.Jenkins;

@Extension
public final class HostedPluginFileSystemWatcher implements RootAction {
  private static final Logger LOGGER =
      Logger.getLogger(HostedPluginFileSystemWatcher.class.getName());

  private static final AutoloadRegistry registry = new AutoloadRegistry();

  private static final ConcurrentHashMap<Path, AutoloadEvent> operationsMap =
      new ConcurrentHashMap<>();

  private static final DelayQueue<AutoloadEvent> operationsQueue = new DelayQueue<>();

  private File getDataDirectory() {
    return new File(Jenkins.get().getRootDir(), "hosted-plugins");
  }

  private File getAutoloadDirectory() {
    return new File(this.getDataDirectory(), "autoload");
  }

  private void createDirectoryIfNotExists(File dir) {
    if (!dir.exists()) {
      try {
        Files.createDirectory(dir.toPath());
      } catch (IOException ex) {
        LOGGER.severe(String.format("Failed to create directory: %s", ex.getMessage()));
      }
    }
  }

  public HostedPluginFileSystemWatcher() {
    /**
     * NOTE(garrett): In practical testing, a full JVM shutdown occurs in a graceful restart. If we
     * were to reload this plugin, the existing FS watcher and daemon thread created within this
     * function would be automatically cleaned up without us having to register JVM shutdown hooks.
     */
    this.onStart();
  }

  @Override
  public String getIconFileName() {
    return null;
  }

  @Override
  public String getDisplayName() {
    return null;
  }

  @Override
  public String getUrlName() {
    return null;
  }

  public void onStart() {
    final var autoloadDir = getAutoloadDirectory();

    final var requiredDirs = List.of(getDataDirectory(), autoloadDir);

    requiredDirs.forEach(dir -> createDirectoryIfNotExists(dir));
    LOGGER.info("All required directories are present or have been created.");

    try (Stream<Path> paths = Files.list(autoloadDir.toPath())) {
      paths
          .filter(path -> path.toFile().isFile() && path.toString().endsWith(".jar"))
          .forEach(
              path -> {
                registry.register(path);
              });
    } catch (IOException ex) {
      LOGGER.warning(
          String.format("Failed to list contents of %s for plugin startup loading", autoloadDir));
    }

    final var dirWatcherDaemon =
        new Thread(new AutoloadDirectoryWatcher(), "Jenkins-Plugin-Host-FS-Watcher");

    dirWatcherDaemon.setDaemon(true);
    dirWatcherDaemon.start();

    final var entryHandler =
        new Thread(new AutoloadEventHandler(), "Jenkins-Plugin-Host-Event-Handler");

    entryHandler.setDaemon(true);
    entryHandler.start();
  }

  private enum AutoloadEventAction {
    REGISTER,
    DEREGISTER,
    RELOAD
  }

  private record AutoloadEvent(AutoloadEventAction action, LocalDateTime expiry, Path path)
      implements Delayed {
    @Override
    public int compareTo(Delayed event) {
      return Long.valueOf(this.getDelay(TimeUnit.NANOSECONDS))
          .compareTo(event.getDelay(TimeUnit.NANOSECONDS));
    }

    @Override
    public long getDelay(TimeUnit unit) {
      /**
       * NOTE(garrett): The DelayQueue<?> contract enforces that this is only called with
       * TimeUnit.NANOSECONDS so we don't have to handle other values.
       */
      return ChronoUnit.NANOS.between(LocalDateTime.now(), this.expiry);
    }
  }

  private class AutoloadEventHandler implements Runnable {
    @Override
    public void run() {
      while (true) {
        try {
          final var event = operationsQueue.take();
          final var path = event.path();
          operationsMap.remove(path);

          final var action = event.action();

          LOGGER.info(
              String.format("Action triggered: %s (%s)", path.toString(), action.toString()));

          switch (action) {
            case REGISTER:
              registry.register(path);
              break;
            case DEREGISTER:
              registry.deregister(path);
              break;
            case RELOAD:
              registry.reload(path);
              break;
          }
        } catch (InterruptedException ex) {
          LOGGER.severe("Event handler thread interrupt, hosted plugins now frozen.");
        }
      }
    }
  }

  private class AutoloadDirectoryWatcher implements Runnable {
    private AutoloadEvent coalesceEvents(AutoloadEvent previous, AutoloadEventAction action) {
      final var path = previous.path();
      final var prevAction = previous.action();
      operationsQueue.remove(previous);

      AutoloadEventAction newAction = null;

      switch (prevAction) {
        case REGISTER:
          switch (action) {
            case REGISTER:
              throw new IllegalStateException("Encountered duplicate registration events");
            case DEREGISTER:
              LOGGER.info(
                  String.format("Registration no longer applicable for %s", path.toString()));

              return null;
            case RELOAD:
              newAction = AutoloadEventAction.REGISTER;
              break;
          }

          break;
        case DEREGISTER:
          switch (action) {
            case REGISTER:
              // TODO(garrett): Some form of hash check to handle
              LOGGER.warning("Re-registration after deletion currently unsupported");

              return null;
            case DEREGISTER:
              throw new IllegalStateException("Encountered duplicate deregistration events");
            case RELOAD:
              throw new IllegalStateException("Encountered modification after deletion");
          }

          break;
        case RELOAD:
          switch (action) {
            case REGISTER:
              throw new IllegalStateException("Encountered registration after modification");
            case DEREGISTER:
              newAction = AutoloadEventAction.DEREGISTER;
              break;
            case RELOAD:
              newAction = AutoloadEventAction.RELOAD;
              break;
          }

          break;
      }

      if (newAction == null) {
        throw new IllegalStateException(
            String.format(
                "Coalescing returned an invalid state (%s followed by %s)",
                prevAction.toString(), action.toString()));
      }

      LOGGER.info(
          String.format(
              "Coalesced existing event for %s (%s, %s => %s)",
              path.toString(), prevAction.toString(), action.toString(), newAction.toString()));

      final var newEvent = new AutoloadEvent(newAction, LocalDateTime.now().plusSeconds(30), path);

      operationsQueue.put(newEvent);
      return newEvent;
    }

    @Override
    public void run() {
      final var autoloadDir = getAutoloadDirectory();
      final var fs = FileSystems.getDefault();

      try (final var watcher = fs.newWatchService()) {
        autoloadDir.toPath().register(watcher, ENTRY_CREATE, ENTRY_DELETE, ENTRY_MODIFY);

        LOGGER.info("Hosted plugin autoloading now operational.");

        while (true) {
          try {
            final var key = watcher.poll(10, TimeUnit.SECONDS);

            if (key == null) {
              continue;
            }

            for (final var event : key.pollEvents()) {
              final var kind = event.kind();

              if (kind == OVERFLOW) {
                LOGGER.warning("Overflow - Some events not delivered");

                continue;
              }

              final var path = autoloadDir.toPath().resolve((Path) event.context());

              final var isDirectory = Files.isDirectory(path);
              final var hasExtension = path.getFileName().toString().endsWith(".jar");

              final var isApplicable = hasExtension && !isDirectory;

              if (!isApplicable) {
                continue;
              }

              final AutoloadEventAction action =
                  (kind == ENTRY_CREATE)
                      ? AutoloadEventAction.REGISTER
                      : (kind == ENTRY_DELETE)
                          ? AutoloadEventAction.DEREGISTER
                          : (kind == ENTRY_MODIFY) ? AutoloadEventAction.RELOAD : null;

              if (action == null) {
                throw new IllegalStateException(
                    String.format("Encountered an unexpected FS event type: %s", kind.toString()));
              }

              operationsMap.compute(
                  path,
                  ((pathKey, mappedEvent) -> {
                    if (mappedEvent != null) {
                      return coalesceEvents(mappedEvent, action);
                    } else {
                      LOGGER.info(
                          String.format(
                              "New event detected for %s (%s)",
                              path.toString(), action.toString()));

                      final var newEvent =
                          new AutoloadEvent(action, LocalDateTime.now().plusSeconds(30), path);

                      operationsQueue.put(newEvent);
                      return newEvent;
                    }
                  }));
            }

            key.reset();
          } catch (InterruptedException ex) {
            LOGGER.severe("Watching thread interrupt, hosted plugins now frozen.");
          }
        }
      } catch (IOException ex) {
        LOGGER.severe(
            String.format("Failed to create autoload directory watcher: %s", ex.getMessage()));
      }
    }
  }
}
