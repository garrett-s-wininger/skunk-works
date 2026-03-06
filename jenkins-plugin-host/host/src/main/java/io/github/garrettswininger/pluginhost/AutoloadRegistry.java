package io.github.garrettswininger.pluginhost;

import hudson.ExtensionPoint;
import hudson.model.ManagementLink;
import io.github.garrettswininger.hosting.DynamicPlugin;
import io.github.garrettswininger.hosting.Hosted;
import io.github.garrettswininger.hosting.ManagementLinkProxy;
import io.github.garrettswininger.hosting.Proxy;
import java.io.File;
import java.io.IOException;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.DigestInputStream;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.logging.Logger;
import jenkins.model.Jenkins;

record Registry(Map<Class<?>, Class<? extends Proxy<?>>> extensions) {
  private static final Logger LOGGER = Logger.getLogger(Registry.class.getName());

  private static Optional<DynamicPlugin<?, ?>> pluginFromEntry(ClassLoader loader, JarEntry entry) {
    final var expectedClassName = entry.getName().replace("/", ".").replace(".class", "");

    Class<?> clazz;

    try {
      clazz = loader.loadClass(expectedClassName);
    } catch (ClassNotFoundException ex) {
      LOGGER.warning(String.format("Failed to find class: %s", expectedClassName));
      return Optional.empty();
    }

    if (!clazz.isAnnotationPresent(Hosted.class)) {
      LOGGER.fine(String.format("Skipping class due to lack of annotation: %s", expectedClassName));
      return Optional.empty();
    }

    if (!DynamicPlugin.class.isAssignableFrom(clazz)) {
      LOGGER.fine(String.format("Skipping class due to invalid type: %s", expectedClassName));
      return Optional.empty();
    }

    DynamicPlugin<?, ?> plugin;

    try {
      plugin = (DynamicPlugin<?, ?>) clazz.getDeclaredConstructor().newInstance();
    } catch (Exception ex) {
      LOGGER.warning(String.format("Failed to instantiate dynamic plugin: %s", expectedClassName));
      return Optional.empty();
    }

    LOGGER.info(
        String.format(
            "Loaded %s extension from JAR: %s",
            plugin.extension.getName(), plugin.implementation.getName()));

    return Optional.of(plugin);
  }

  // NOTE(garrett): We probably want a bi-directional map here but don't have
  // built-ins for that so we'll just use static maps for now while we're proving
  // out the design.
  static final Map<
          Class<? extends ExtensionPoint>, Class<? extends Proxy<? extends ExtensionPoint>>>
      EXTENSION_TYPE_MAP = Map.of(ManagementLink.class, ManagementLinkProxy.class);

  static final Map<Class<? extends Proxy<? extends ExtensionPoint>>, Class<? extends ExtensionPoint>>
      PROXY_TYPE_MAP = Map.of(ManagementLinkProxy.class, ManagementLink.class);

  public static Optional<Registry> fromJar(File file, URLClassLoader loader) {
    try (final var jar = new JarFile(file)) {
      final var registry = new Registry(new HashMap<>());

      jar.stream()
          .filter(entry -> entry.getName().endsWith(".class"))
          .map(entry -> Registry.pluginFromEntry(loader, entry))
          .filter(Optional::isPresent)
          .map(Optional::get)
          .forEach(
              plugin -> {
                for (final var entry : EXTENSION_TYPE_MAP.entrySet()) {
                  if (entry.getKey().isAssignableFrom(plugin.extension)) {
                    registry.extensions().put(plugin.implementation, entry.getValue());
                    return;
                  }
                }

                LOGGER.warning(
                    "Skipping class due to unsupported extension type: %s"
                        .formatted(plugin.extension.getName()));
              });

      return Optional.of(registry);
    } catch (Exception ex) {
      LOGGER.warning("Failed to load registry from JAR: %s".formatted(file.getPath()));
      return Optional.empty();
    }
  }
}

record RegisteredJar(byte[] digest, URLClassLoader loader, Registry registry) {
  public static Optional<RegisteredJar> fromFile(File file) throws IOException {
    final MessageDigest digestDriver;

    try {
      digestDriver = MessageDigest.getInstance("SHA-256");
    } catch (NoSuchAlgorithmException ex) {
      // NOTE(garrett): SHA-256 is guaranteed to be available according to the
      // specification so this is effectively an error that isn't reasonable to
      // handle (Java 8 - 25).
      throw new RuntimeException(ex);
    }

    try (final var input = Files.newInputStream(file.toPath());
        final var digestStream = new DigestInputStream(input, digestDriver)) {
      final var buffer = new byte[4096];

      while (digestStream.read(buffer, 0, buffer.length) != -1) {
        // NOTE(garrett): Don't need to do anything, just pass data
        // through the stream to update the digest
      }
    }

    final var loader =
        new URLClassLoader(new URL[] {file.toURI().toURL()}, RegisteredJar.class.getClassLoader());

    final var registry = Registry.fromJar(file, loader);

    if (registry.isEmpty()) {
      return Optional.empty();
    }

    return Optional.of(new RegisteredJar(digestDriver.digest(), loader, registry.get()));
  }
}

class AutoloadRegistry {
  private static final Logger LOGGER = Logger.getLogger(AutoloadRegistry.class.getName());

  private Map<Path, RegisteredJar> registrations;

  AutoloadRegistry() {
    this.registrations = new HashMap<>();
  }

  @SuppressWarnings({"unchecked"})
  void deregister(Path pluginPath) {
    LOGGER.info(String.format("Deregistering: %s", pluginPath.toString()));

    if (!this.registrations.containsKey(pluginPath)) {
      LOGGER.fine(
          "No registered classes for %s, nothing to be done".formatted(pluginPath.toString()));
      return;
    }

    final var registration = this.registrations.get(pluginPath);

    registration
        .registry()
        .extensions()
        .forEach(
            (implementationClass, proxyClass) -> {
              final var extensionPoint = Registry.PROXY_TYPE_MAP.get(proxyClass);

              if (extensionPoint == null) {
                LOGGER.warning("No extension point for proxy: %s".formatted(proxyClass.getName()));
                return;
              }
              final var extensionList =
                  (List<ExtensionPoint>) Jenkins.get().getExtensionList(extensionPoint);

              for (final var extension : extensionList) {
                if (proxyClass.isAssignableFrom(extension.getClass())) {
                  final var proxy = (Proxy<?>) extension;

                  if (registration
                      .loader()
                      .equals(proxy.getImplementation().getClass().getClassLoader())) {
                    LOGGER.info(
                        "Removing proxy for %s: %s"
                            .formatted(
                                implementationClass.getName(), extension.getClass().getName()));
                    extensionList.remove(extension);
                    break;
                  }
                }
              }
            });

    try {
      registration.loader().close();
    } catch (IOException ex) {
      LOGGER.warning(
          "Failed to close class loader for %s: %s".formatted(pluginPath, ex.getMessage()));
    }

    this.registrations.remove(pluginPath);
  }

  @SuppressWarnings({"unchecked"})
  void register(Path pluginPath) {
    LOGGER.info(String.format("Registering: %s", pluginPath.toString()));
    final Optional<RegisteredJar> registration;

    try {
      registration = RegisteredJar.fromFile(pluginPath.toFile());
    } catch (IOException ex) {
      LOGGER.warning("Failed to read plugin JAR: %s".formatted(pluginPath.toString()));
      return;
    }

    if (registration.isEmpty()) {
      LOGGER.warning("Failed to register plugin JAR: %s".formatted(pluginPath.toString()));
      return;
    }

    final var registeredJar = registration.get();

    registeredJar
        .registry()
        .extensions()
        .forEach(
            (implementationClass, proxyClass) -> {
              final var extensionPoint = Registry.PROXY_TYPE_MAP.get(proxyClass);

              if (extensionPoint == null) {
                LOGGER.warning("No extension point for proxy: %s".formatted(proxyClass.getName()));
                return;
              }

              final var extensionList =
                  (List<ExtensionPoint>) Jenkins.get().getExtensionList(extensionPoint);

              final Proxy<?> proxyInstance;

              try {
                final var impl = implementationClass.getDeclaredConstructor().newInstance();
                proxyInstance =
                    proxyClass
                        .getDeclaredConstructor(extensionPoint)
                        .newInstance(extensionPoint.cast(impl));

                extensionList.add((ExtensionPoint) proxyInstance);
              } catch (Exception ex) {
                LOGGER.warning(
                    "Failed to create extension: %s".formatted(implementationClass.getName()));
                return;
              }

              LOGGER.info(
                  "Added %s extension: %s"
                      .formatted(
                          extensionPoint.getName(),
                          proxyInstance.getImplementation().getClass().getName()));
            });

    registrations.put(pluginPath, registeredJar);
  }

  void reload(Path pluginPath) {
    final var currentRegistration = this.registrations.get(pluginPath);

    if (currentRegistration == null) {
      register(pluginPath);
      return;
    }

    LOGGER.info("Re-registering: %s".formatted(pluginPath.toString()));
    final Optional<RegisteredJar> newRegistration;

    try {
      newRegistration = RegisteredJar.fromFile(pluginPath.toFile());
    } catch (IOException ex) {
      LOGGER.warning("Failed to read plugin JAR: %s".formatted(pluginPath.toString()));
      return;
    }

    if (newRegistration.isEmpty()) {
      LOGGER.warning("Failed to reload plugin JAR: %s".formatted(pluginPath.toString()));
      return;
    }

    final var registration = newRegistration.get();
    final var newDigest = registration.digest();
    final var existingDigest = currentRegistration.digest();

    if (newDigest.length == 0 || existingDigest.length == 0) {
      LOGGER.warning(
          String.format(
              "Checkum calculation was unable to be completed, %s will" + " not be reloaded",
              pluginPath.toString()));
      return;
    }

    if (Boolean.parseBoolean(System.getProperty("pluginHost.reload.hashCheck"))) {
      if (Arrays.equals(newDigest, existingDigest)) {
        LOGGER.info(
            "Old and new JAR hashes at %s are identical, nothing to do"
                .formatted(pluginPath.toString()));
        return;
      }
    }

    if (registration.registry().extensions().size()
        != currentRegistration.registry().extensions().size()) {
      LOGGER.warning(
          "The number of extensions has changed, %s will not be reloaded"
              .formatted(pluginPath.toString()));
      return;
    }

    currentRegistration
        .registry()
        .extensions()
        .forEach(
            (implementationClass, proxyClass) -> {
              // TODO(garrett): Implement reloading of extensions
            });
  }
}
