package io.github.garrettswininger.pluginhost;

import hudson.ExtensionPoint;
import io.github.garrettswininger.hosting.DynamicPlugin;
import io.github.garrettswininger.hosting.Hosted;
import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.logging.Logger;
import jenkins.model.Jenkins;

record HostedRegistration(
    URLClassLoader loader,
    Map<Class<? extends ExtensionPoint>, List<? extends ExtensionPoint>> extensions
) {}

class AutoloadRegistry {
    private final static Logger LOGGER = Logger.getLogger(
        AutoloadRegistry.class.getName()
    );

    private Map<Path, HostedRegistration> registrations;

    AutoloadRegistry() {
        this.registrations = new HashMap<>();
    }

    private
    Optional<DynamicPlugin<? extends ExtensionPoint, ? extends ExtensionPoint>>
    entryToPlugin(ClassLoader loader, JarEntry entry) {
        final var expectedClassName = entry.getName()
            .replace("/", ".")
            .replace(".class", "");

        Class<?> clazz;

        try {
            clazz = loader.loadClass(expectedClassName);
        } catch (ClassNotFoundException ex) {
            LOGGER.warning(
                String.format(
                    "Failed to find class: %s",
                    expectedClassName
                )
            );

            return Optional.empty();
        }

        if (!clazz.isAnnotationPresent(Hosted.class)) {
            LOGGER.fine(
                String.format(
                    "Skipping class due to lack of annotation: %s",
                    expectedClassName
                )
            );

            return Optional.empty();
        }

        if (!DynamicPlugin.class.isAssignableFrom(clazz)) {
            LOGGER.fine(
                String.format(
                    "Skipping class due to invalid type: %s",
                    expectedClassName
                )
            );

            return Optional.empty();
        }

        DynamicPlugin<? extends ExtensionPoint, ? extends ExtensionPoint> plugin;

        try {
            plugin = (DynamicPlugin)clazz
                .getDeclaredConstructor().newInstance();
        } catch (Exception ex) {
            LOGGER.warning(
                String.format(
                    "Failed to instantiate dynamic plugin: %s",
                    expectedClassName
                )
            );

            return Optional.empty();
        }

        LOGGER.info(
            String.format(
                "Loaded %s extension: %s",
                plugin.extension.getName(),
                plugin.implementation.getName()
            )
        );

        return Optional.of(plugin);
    }

    void deregister(Path pluginPath) {
        LOGGER.info(String.format("Deregistering: %s", pluginPath.toString()));

        if (!this.registrations.containsKey(pluginPath)) {
            LOGGER.fine("No registered classes, nothing to be done");
            return;
        }

        final var registration = this.registrations.get(pluginPath);
        final var extensions = registration.extensions();

        for (final var extensionType: extensions.keySet()) {
            final var extensionList =
                Jenkins.get().getExtensionList(extensionType);

            for (final var extension: extensions.get(extensionType)) {
                extensionList.remove(extension);

                LOGGER.info(
                    String.format(
                        "Removed %s extension: %s",
                        extensionType.getName(),
                        extension.getClass().getName()
                    )
                );
            }
        }

        try {
            registration.loader().close();
        } catch (IOException ex) {
            LOGGER.warning(
                String.format(
                    "Failed to close class loader for %s: %s",
                    pluginPath,
                    ex.getMessage()
                )
            );
        }

        this.registrations.remove(pluginPath);
    }

    void register(Path pluginPath) {
        LOGGER.info(
            String.format(
                "Registering: %s",
                pluginPath.toString()
            )
        );

        final var file = pluginPath.toFile();
        final URLClassLoader loader;

        try {
            loader = new URLClassLoader(
                new URL[]{file.toURI().toURL()},
                this.getClass().getClassLoader()
            );
        } catch (MalformedURLException ex) {
            LOGGER.warning(
                String.format(
                    "Could not register plugins for path, malformed URL: %s",
                    pluginPath.toString()
                )
            );

            return;
        }

        final Map<
            Class<? extends ExtensionPoint>, List<? extends ExtensionPoint>
        > pathRegistrations = new HashMap<>();

        try (var jar = new JarFile(file)) {
            jar.stream()
                .filter(entry -> entry.getName().endsWith(".class"))
                .map(entry -> entryToPlugin(loader, entry))
                .filter(entry -> entry.isPresent())
                .forEach(entry -> {
                    final var plugin = entry.get();
                    ExtensionPoint instance;

                    try {
                        instance = plugin.getInstance();
                    } catch (Exception ex) {
                        LOGGER.warning(
                            String.format(
                                "Failed to instantiate extension (%s): %s",
                                plugin.implementation.getName(),
                                ex.getMessage()
                            )
                        );

                        return;
                    }

                    if (pathRegistrations.containsKey(plugin.extension)) {
                        final var registeredExtensions =
                            pathRegistrations.get(plugin.extension);

                        ((List<ExtensionPoint>)registeredExtensions).add(instance);
                    } else {
                        final List<ExtensionPoint> instances =
                            new ArrayList<>();

                        instances.add(instance);
                        pathRegistrations.put(plugin.extension, instances);
                    }

                    final var extensionList = Jenkins.get()
                        .getExtensionList((Class<ExtensionPoint>)plugin.extension);

                    extensionList.add(instance);
                });
        } catch (IOException ex) {
            LOGGER.warning(
                String.format(
                    "Failed to access JAR (%s): %s",
                    pluginPath.toString(),
                    ex.getMessage()
                )
            );
        }

        this.registrations.put(
            pluginPath,
            new HostedRegistration(
                loader,
                pathRegistrations
            )
        );
    }

    void reload(Path pluginPath) {
        LOGGER.info(
            String.format("Re-registering: %s", pluginPath.toString())
        );
    }
}
