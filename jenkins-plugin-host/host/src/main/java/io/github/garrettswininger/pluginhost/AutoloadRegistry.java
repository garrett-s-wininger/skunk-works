package io.github.garrettswininger.pluginhost;

import io.github.garrettswininger.hosting.DynamicPlugin;
import io.github.garrettswininger.hosting.Hosted;
import java.io.File;
import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.DigestInputStream;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.logging.Level;
import java.util.logging.Logger;

class AutoloadRegistry {
  private static final Logger LOGGER = Logger.getLogger(AutoloadRegistry.class.getName());

  private final Map<Path, HostedRegistration> registrations;
  private final Map<Class<?>, ExtensionAdapter<?, ?>> adapters;

  AutoloadRegistry() {
    this.registrations = new HashMap<>();
    this.adapters = new HashMap<>();

    final ExtensionAdapter<?, ?> managementLinkAdapter = new ManagementLinkAdapter();
    this.adapters.put(managementLinkAdapter.extensionType(), managementLinkAdapter);
    final ExtensionAdapter<?, ?> stepDescriptorAdapter = new StepDescriptorAdapter();
    this.adapters.put(stepDescriptorAdapter.extensionType(), stepDescriptorAdapter);
  }

  private byte[] calculateDigest(Path path) {
    try {
      final var digest = MessageDigest.getInstance("MD5");

      try (final var input = Files.newInputStream(path);
          final var digestStream = new DigestInputStream(input, digest)) {
        final var buffer = new byte[4096];

        while (digestStream.read(buffer, 0, buffer.length) != -1) {
          // NOTE(garrett): Don't need to do anything, just pass data
          // through the stream to update the digest
        }

        return digest.digest();
      } catch (IOException ex) {
        LOGGER.warning(
            String.format(
                "Unable to access contents of %s for digest calculation", path.toString()));

        return new byte[0];
      }
    } catch (NoSuchAlgorithmException ex) {
      LOGGER.warning(
          "Unable to instantiate the requested hash provider, cannot" + " validate file checksums");

      return new byte[0];
    }
  }

  // NOTE(garrett): We have to do unchecked casts here as we need to manually
  // reify extension types at runtime
  @SuppressWarnings("unchecked")
  private Optional<DynamicPlugin<?, ?>> entryToPlugin(ClassLoader loader, JarEntry entry) {
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

  private Optional<PreparedRegistration> createRegistrationFromJar(File file) {
    final URLClassLoader loader;

    try {
      loader =
          new URLClassLoader(new URL[] {file.toURI().toURL()}, this.getClass().getClassLoader());
    } catch (MalformedURLException ex) {
      LOGGER.warning(
          String.format(
              "Could not register plugins for path, malformed URL: %s", file.getPath().toString()));

      return Optional.empty();
    }

    final Map<Class<?>, List<Object>> discovered = new HashMap<>();

    try (var jar = new JarFile(file)) {
      jar.stream()
          .filter(entry -> entry.getName().endsWith(".class"))
          .map(entry -> entryToPlugin(loader, entry))
          .forEach(
              entry -> {
                entry.ifPresent(
                    plugin -> {
                      final var adapter = this.adapters.get(plugin.extension);

                      if (adapter == null) {
                        LOGGER.fine(
                            String.format(
                                "Skipping unsupported extension type for hosted loading: %s",
                                plugin.extension.getName()));
                        return;
                      }

                      final Object instance;

                      try {
                        instance = plugin.getInstance();
                      } catch (Exception ex) {
                        LOGGER.log(
                            Level.WARNING,
                            String.format(
                                "Failed to instantiate extension (%s)",
                                plugin.implementation.getName()),
                            ex);

                        return;
                      }

                      if (!plugin.extension.isInstance(instance)) {
                        LOGGER.warning(
                            String.format(
                                "Skipping extension due to incompatible instance type: %s",
                                instance.getClass().getName()));
                        return;
                      }

                      discovered
                          .computeIfAbsent(plugin.extension, ignored -> new ArrayList<>())
                          .add(instance);
                    });
              });
    } catch (IOException ex) {
      LOGGER.warning(
          String.format(
              "Failed to access JAR (%s): %s", file.getPath().toString(), ex.getMessage()));

      try {
        loader.close();
      } catch (IOException closeEx) {
        LOGGER.warning(
            String.format(
                "Failed to close class loader for %s after load failure: %s",
                file.getPath(), closeEx.getMessage()));
      }

      return Optional.empty();
    }

    if (discovered.isEmpty()) {
      LOGGER.info(String.format("No supported hosted extensions found in %s", file.getPath()));

      try {
        loader.close();
      } catch (IOException ex) {
        LOGGER.warning(
            String.format(
                "Failed to close class loader for %s with no supported extensions: %s",
                file.getPath(), ex.getMessage()));
      }

      return Optional.empty();
    }

    final Map<Class<?>, List<?>> delegates = new HashMap<>();
    discovered.forEach((type, instances) -> delegates.put(type, List.copyOf(instances)));

    return Optional.of(new PreparedRegistration(calculateDigest(file.toPath()), loader, delegates));
  }

  @SuppressWarnings({"rawtypes", "unchecked"})
  private RegisteredExtension registerWithAdapter(
      ExtensionAdapter<?, ?> adapter, List<?> delegates) {
    final List wrappers = ((ExtensionAdapter) adapter).registerStable((List) delegates);
    return new RegisteredExtension(adapter, wrappers);
  }

  @SuppressWarnings({"rawtypes", "unchecked"})
  private void swapWithAdapter(RegisteredExtension registeredExtension, List<?> newDelegates) {
    ((ExtensionAdapter) registeredExtension.adapter())
        .swap((List) registeredExtension.wrappers(), (List) newDelegates);
  }

  @SuppressWarnings({"rawtypes", "unchecked"})
  private void deregisterWithAdapter(RegisteredExtension registeredExtension) {
    ((ExtensionAdapter) registeredExtension.adapter())
        .deregister((List) registeredExtension.wrappers());
  }

  void deregister(Path pluginPath) {
    LOGGER.info(String.format("Deregistering: %s", pluginPath.toString()));

    final var registration = this.registrations.get(pluginPath);

    if (registration == null) {
      LOGGER.fine("No registered classes, nothing to be done");
      return;
    }

    registration
        .extensions()
        .forEach((ignored, registeredExtension) -> deregisterWithAdapter(registeredExtension));

    try {
      registration.loader().close();
    } catch (IOException ex) {
      LOGGER.warning(
          String.format("Failed to close class loader for %s: %s", pluginPath, ex.getMessage()));
    }

    this.registrations.remove(pluginPath);
  }

  void register(Path pluginPath) {
    LOGGER.info(String.format("Registering: %s", pluginPath.toString()));

    final var preparedRegistration = this.createRegistrationFromJar(pluginPath.toFile());

    preparedRegistration.ifPresent(
        loadedRegistration -> {
          final Map<Class<?>, RegisteredExtension> registered = new HashMap<>();

          loadedRegistration
              .delegates()
              .forEach(
                  (extensionType, delegates) -> {
                    final var adapter = this.adapters.get(extensionType);

                    if (adapter == null) {
                      LOGGER.warning(
                          String.format(
                              "No adapter found during registration for extension type: %s",
                              extensionType.getName()));
                      return;
                    }

                    final var registeredExtension = registerWithAdapter(adapter, delegates);
                    registered.put(extensionType, registeredExtension);

                    delegates.forEach(
                        delegate ->
                            LOGGER.info(
                                String.format(
                                    "%s extension successfully installed into the instance",
                                    delegate.getClass().getName())));
                  });

          if (registered.isEmpty()) {
            LOGGER.warning(
                String.format(
                    "No extensions were registered from %s after adapter filtering", pluginPath));

            try {
              loadedRegistration.loader().close();
            } catch (IOException ex) {
              LOGGER.warning(
                  String.format(
                      "Failed to close class loader for %s after empty registration: %s",
                      pluginPath, ex.getMessage()));
            }

            return;
          }

          this.registrations.put(
              pluginPath,
              new HostedRegistration(
                  loadedRegistration.digest(), loadedRegistration.loader(), registered));
        });
  }

  void reload(Path pluginPath) {
    final var currentRegistration = this.registrations.get(pluginPath);

    if (currentRegistration == null) {
      register(pluginPath);
      return;
    }

    LOGGER.info(String.format("Re-registering: %s", pluginPath.toString()));

    final var newDigest = calculateDigest(pluginPath);
    final var existingDigest = currentRegistration.digest();

    if (newDigest.length == 0 || existingDigest.length == 0) {
      LOGGER.warning(
          String.format(
              "Checkum calculation was unable to be completed, %s will" + " not be reloaded",
              pluginPath.toString()));
    }

    if (Arrays.equals(newDigest, existingDigest)) {
      LOGGER.info(
          String.format(
              "Old and new JAR hashes at %s are identical, nothing to do", pluginPath.toString()));

      return;
    }

    final var updatedRegistration = this.createRegistrationFromJar(pluginPath.toFile());

    if (updatedRegistration.isEmpty()) {
      LOGGER.warning(String.format("Failed to load updated plugin from %s", pluginPath.toString()));

      return;
    }

    final var newRegistration = updatedRegistration.get();

    if (!newRegistration.delegates().keySet().equals(currentRegistration.extensions().keySet())) {
      LOGGER.warning(
          String.format("Cannot atomically reload %s because extension types changed", pluginPath));

      try {
        newRegistration.loader().close();
      } catch (IOException ex) {
        LOGGER.warning(
            String.format(
                "Failed to close class loader for rejected reload at %s: %s",
                pluginPath, ex.getMessage()));
      }

      return;
    }

    for (final var extensionType : currentRegistration.extensions().keySet()) {
      final var registered = currentRegistration.extensions().get(extensionType);
      final var newDelegates = newRegistration.delegates().get(extensionType);

      if (newDelegates == null || registered.wrappers().size() != newDelegates.size()) {
        LOGGER.warning(
            String.format(
                "Cannot atomically reload %s because extension count changed for %s (%d => %d)",
                pluginPath,
                extensionType.getName(),
                registered.wrappers().size(),
                newDelegates == null ? 0 : newDelegates.size()));

        try {
          newRegistration.loader().close();
        } catch (IOException ex) {
          LOGGER.warning(
              String.format(
                  "Failed to close class loader for rejected reload at %s: %s",
                  pluginPath, ex.getMessage()));
        }

        return;
      }
    }

    for (final var extensionType : currentRegistration.extensions().keySet()) {
      final var registered = currentRegistration.extensions().get(extensionType);
      final var newDelegates = newRegistration.delegates().get(extensionType);
      swapWithAdapter(registered, newDelegates);
    }

    try {
      currentRegistration.loader().close();
    } catch (IOException ex) {
      LOGGER.warning(
          String.format(
              "Failed to close class loader for previous registration at %s: %s",
              pluginPath.toString(), ex.getMessage()));
    }

    this.registrations.put(
        pluginPath,
        new HostedRegistration(
            newRegistration.digest(), newRegistration.loader(), currentRegistration.extensions()));

    LOGGER.info(String.format("Re-registration complete: %s", pluginPath.toString()));
  }
}
