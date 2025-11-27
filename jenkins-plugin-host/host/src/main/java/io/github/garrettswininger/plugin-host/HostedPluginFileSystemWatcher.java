package io.github.garrettswininger.pluginhost;

import hudson.Extension;
import hudson.model.RootAction;
import java.io.File;
import java.io.IOException;
import java.nio.file.FileSystems;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardWatchEventKinds;
import java.nio.file.WatchEvent;
import java.time.LocalDateTime;
import java.time.temporal.ChronoUnit;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Delayed;
import java.util.concurrent.DelayQueue;
import java.util.concurrent.TimeUnit;
import java.util.List;
import java.util.logging.Logger;
import jenkins.model.Jenkins;

import static java.nio.file.StandardWatchEventKinds.ENTRY_CREATE;
import static java.nio.file.StandardWatchEventKinds.ENTRY_DELETE;
import static java.nio.file.StandardWatchEventKinds.ENTRY_MODIFY;
import static java.nio.file.StandardWatchEventKinds.OVERFLOW;

@Extension
public final class HostedPluginFileSystemWatcher implements RootAction {
    private final static Logger LOGGER = Logger.getLogger(
        HostedPluginFileSystemWatcher.class.getName()
    );

    private final static AutoloadRegistry registry = new AutoloadRegistry();

    private final static ConcurrentHashMap<Path, AutoloadEvent> operationsMap
        = new ConcurrentHashMap<>();

    private final static DelayQueue<AutoloadEvent> operationsQueue
        = new DelayQueue<>();

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
                LOGGER.severe(
                    String.format("Failed to create directory: %s", ex.getMessage())
                );
            }
        }
    }

    public HostedPluginFileSystemWatcher() {
        /**
         * NOTE(garrett): In practical testing, a full JVM shutdown occurs in a
         * graceful restart. If we were to reload this plugin, the existing
         * FS watcher and daemon thread created within this function would be
         * automatically cleaned up without us having to register JVM shutdown
         * hooks.
         */
        this.onStart();
    }

    @Override
    public String getIconFileName() { return null; }

    @Override
    public String getDisplayName() { return null; }

    @Override
    public String getUrlName() { return null; }

    public void onStart() {
        final var requiredDirs = List.of(
            getDataDirectory(),
            getAutoloadDirectory()
        );

        requiredDirs.forEach(dir -> createDirectoryIfNotExists(dir));
        LOGGER.info("All required directories are present or have been created.");

        final var dirWatcherDaemon = new Thread(
            new AutoloadDirectoryWatcher(),
            "Jenkins-Plugin-Host-FS-Watcher"
        );

        dirWatcherDaemon.setDaemon(true);
        dirWatcherDaemon.start();

        final var entryHandler = new Thread(
            new AutoloadEventHandler(),
            "Jenkins-Plugin-Host-Event-Handler"
        );

        entryHandler.setDaemon(true);
        entryHandler.start();
    }

    private enum AutoloadEventAction {
        REGISTER,
        DEREGISTER,
        RELOAD
    }

    private record AutoloadEvent(
                AutoloadEventAction action, LocalDateTime expiry, Path path
            ) implements Delayed {
        @Override
        public int compareTo(Delayed event) {
            return Long.valueOf(this.getDelay(TimeUnit.NANOSECONDS))
                .compareTo(event.getDelay(TimeUnit.NANOSECONDS));
        }

        @Override
        public long getDelay(TimeUnit unit) {
            /**
             * NOTE(garrett): The DelayQueue<?> contract enforces that this is
             * only called with TimeUnit.NANOSECONDS so we don't have to handle
             * other values.
             */
            return ChronoUnit.NANOS.between(
                LocalDateTime.now(),
                this.expiry
            );
        }
    }

    private class AutoloadEventHandler implements Runnable {
        @Override
        public void run() {
            while (true) {
                try {
                    final var event = operationsQueue.take();
                    final var action = event.action();

                    // TODO(garrett): Actually do useful things
                    switch (action) {
                        default:
                            LOGGER.info(
                                String.format(
                                    "OBTAINED ACTION: %s (%s)\n",
                                    event.path().toString(),
                                    action.toString()
                                )
                            );

                            break;
                    }
                } catch (InterruptedException ex) {
                    LOGGER.severe(
                        "Event handler thread interrupt, hosted plugins now frozen."
                    );
                }
            }
        }
    }

    private class AutoloadDirectoryWatcher implements Runnable {
        @Override
        public void run() {
            final var autoloadDir = getAutoloadDirectory();
            final var fs = FileSystems.getDefault();

            try (final var watcher = fs.newWatchService()) {
                autoloadDir.toPath().register(
                    watcher,
                    ENTRY_CREATE,
                    ENTRY_DELETE,
                    ENTRY_MODIFY
                );

                LOGGER.info("Hosted plugin autoloading now operational.");

                while (true) {
                    try {
                        final var key = watcher.poll(10, TimeUnit.SECONDS);

                        if (key == null) {
                            continue;
                        }

                        for (final var event: key.pollEvents()) {
                            final var kind = event.kind();

                            if (kind == OVERFLOW) {
                                LOGGER.warning(
                                    "Overflow - Some events not delivered"
                                );

                                continue;
                            }

                            final var path = autoloadDir.toPath().resolve(
                                (Path)event.context()
                            );

                            final var isDirectory = Files.isDirectory(path);
                            final var hasExtension = path.getFileName()
                                .toString().endsWith(".jar");

                            final var isApplicable = hasExtension && !isDirectory;

                            if (!isApplicable) {
                                continue;
                            }

                            final AutoloadEventAction action =
                                (kind == ENTRY_CREATE) ? AutoloadEventAction.REGISTER :
                                (kind == ENTRY_DELETE) ? AutoloadEventAction.DEREGISTER :
                                (kind == ENTRY_MODIFY) ? AutoloadEventAction.RELOAD :
                                null;

                            if (action == null) {
                                throw new IllegalStateException(
                                    String.format(
                                        "Encountered an unexpected event type: %s",
                                        kind.toString()
                                    )
                                );
                            }

                            operationsMap.compute(path, ((pathKey, mappedEvent) -> {
                                // FIXME(garrett): Actually compute
                                if (mappedEvent != null) {
                                    LOGGER.info("PRESENT!");
                                    return mappedEvent;
                                } else {
                                    LOGGER.info("MISSING!");

                                    final var newEvent = new AutoloadEvent(
                                        action,
                                        LocalDateTime.now().plusSeconds(30),
                                        path
                                    );

                                    operationsQueue.put(newEvent);
                                    return newEvent;
                                }
                            }));
                        }

                        key.reset();
                    } catch (InterruptedException ex) {
                        LOGGER.severe(
                            "Watching thread interrupt, hosted plugins now frozen."
                        );
                    }
                }
            } catch (IOException ex) {
                LOGGER.severe(
                    String.format(
                        "Failed to create autoload directory watcher: %s",
                        ex.getMessage()
                    )
                );
            }
        }
    }
}
