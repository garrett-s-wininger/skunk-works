package io.github.garrettswininger.pluginhost;

import hudson.Plugin;
import java.io.File;
import java.io.IOException;
import java.lang.Thread;
import java.nio.file.FileSystems;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardWatchEventKinds;
import java.nio.file.WatchEvent;
import java.util.concurrent.TimeUnit;
import java.util.List;
import java.util.logging.Logger;

import static java.nio.file.StandardWatchEventKinds.ENTRY_CREATE;
import static java.nio.file.StandardWatchEventKinds.ENTRY_DELETE;
import static java.nio.file.StandardWatchEventKinds.ENTRY_MODIFY;
import static java.nio.file.StandardWatchEventKinds.OVERFLOW;

public class PluginHost extends Plugin {
    private final static Logger LOGGER = Logger.getLogger(
        PluginHost.class.getName()
    );

    void createDirectoryIfNotExists(File dir) {
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

    void deregisterHostedPlugin(Path pluginPath) {
        LOGGER.info(String.format("Deregistering: %s", pluginPath.toString()));
    }

    void registerHostedPlugin(Path pluginPath) {
        LOGGER.info(
            String.format(
                "Registering: %s",
                pluginPath.getFileName()
            )
        );
    }

    void reloadHostedPlugin(Path pluginPath) {
        LOGGER.info(
            String.format("Re-registering: %s", pluginPath.toString())
        );
    }

    @Override
    public void start() {
        final var dataDir = Utility.getDataDirectory();
        final var autoloadDir = Utility.getAutoloadDirectory();
        final var requiredDirs = List.of(dataDir, autoloadDir);

        requiredDirs.forEach(dir -> createDirectoryIfNotExists(dir));
        LOGGER.info("All required directories are present or have been created.");

        final var fs = FileSystems.getDefault();

        final var dirWatcherDaemon = new Thread(() -> {
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

                        // NOTE(garrett): We'll want to actually re-order the
                        // events here to handle deletions first. In the event
                        // of a rename, we get a creation and deletion but these
                        // may not be ordered from the underlying watch system
                        for (final var event: key.pollEvents()) {
                            final var kind = event.kind();
                            final var path = autoloadDir.toPath().resolve(
                                (Path)event.context()
                            );

                            final var isDirectory = Files.isDirectory(path);
                            final var hasExtension = path.getFileName()
                                .toString().endsWith(".jar");

                            final var isApplicable = hasExtension && !isDirectory;

                            if ((kind == ENTRY_CREATE || kind == ENTRY_MODIFY)
                                    && !isApplicable) {
                                continue;
                            }

                            if (kind == ENTRY_CREATE) {
                                registerHostedPlugin(path);
                            } else if (kind == ENTRY_DELETE) {
                                deregisterHostedPlugin(path);
                            } else if (kind == ENTRY_MODIFY) {
                                reloadHostedPlugin(path);
                            } else if (kind == OVERFLOW) {
                                LOGGER.info(
                                    "Overflow - Some events not delivered"
                                );
                            }
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
        });

        dirWatcherDaemon.setName("Plugin Host Filesystem Watcher");
        dirWatcherDaemon.setDaemon(true);
        dirWatcherDaemon.start();
    }
}
