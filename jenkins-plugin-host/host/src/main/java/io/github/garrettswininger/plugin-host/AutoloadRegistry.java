package io.github.garrettswininger.pluginhost;

import hudson.ExtensionComponent;
import hudson.ExtensionList;
import hudson.ExtensionPoint;
import hudson.model.Describable;
import hudson.model.Descriptor;
import io.github.garrettswininger.hosting.DynamicPlugin;
import io.github.garrettswininger.hosting.Hosted;
import java.io.File;
import java.io.IOException;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
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
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.logging.Logger;
import jenkins.model.Jenkins;

record HostedRegistration(
    byte[] digest,
    URLClassLoader loader,
    Map<Class<? extends ExtensionPoint>, List<? extends ExtensionPoint>> extensions) {}

class AutoloadRegistry {
  private static final Logger LOGGER = Logger.getLogger(AutoloadRegistry.class.getName());

  private Map<Path, HostedRegistration> registrations;

  AutoloadRegistry() {
    this.registrations = new HashMap<>();
  }

  private boolean isSupportedExtensionType(Class<? extends ExtensionPoint> extensionType) {
    return !Descriptor.class.isAssignableFrom(extensionType)
        && !Describable.class.isAssignableFrom(extensionType);
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
  private Optional<DynamicPlugin<? extends ExtensionPoint, ? extends ExtensionPoint>> entryToPlugin(
      ClassLoader loader, JarEntry entry) {
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

    DynamicPlugin<? extends ExtensionPoint, ? extends ExtensionPoint> plugin;

    try {
      plugin = (DynamicPlugin) clazz.getDeclaredConstructor().newInstance();
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

  // NOTE(garrett): We must manually reify types at runtime for loaded
  // extensions
  @SuppressWarnings("unchecked")
  private Optional<HostedRegistration> createRegistrationFromJar(File file) {
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

    final Map<Class<? extends ExtensionPoint>, List<? extends ExtensionPoint>> pathRegistrations =
        new HashMap<>();

    try (var jar = new JarFile(file)) {
      jar.stream()
          .filter(entry -> entry.getName().endsWith(".class"))
          .map(entry -> entryToPlugin(loader, entry))
          .forEach(
              entry -> {
                entry.ifPresent(
                    plugin -> {
                      if (!isSupportedExtensionType(plugin.extension)) {
                        LOGGER.warning(
                            String.format(
                                "Skipping unsupported extension type for hosted loading: %s",
                                plugin.extension.getName()));

                        return;
                      }

                      ExtensionPoint instance;

                      try {
                        instance = plugin.getInstance();
                      } catch (Exception ex) {
                        LOGGER.warning(
                            String.format(
                                "Failed to instantiate extension (%s): %s",
                                plugin.implementation.getName(), ex.getMessage()));

                        return;
                      }

                      if (pathRegistrations.containsKey(plugin.extension)) {
                        final var registeredExtensions = pathRegistrations.get(plugin.extension);

                        ((List<ExtensionPoint>) registeredExtensions).add(instance);
                      } else {
                        final List<ExtensionPoint> instances = new ArrayList<>();

                        instances.add(instance);
                        pathRegistrations.put(plugin.extension, instances);
                      }
                    });
              });
    } catch (IOException ex) {
      LOGGER.warning(
          String.format(
              "Failed to access JAR (%s): %s", file.getPath().toString(), ex.getMessage()));

      return Optional.empty();
    }

    return Optional.of(
        new HostedRegistration(calculateDigest(file.toPath()), loader, pathRegistrations));
  }

  void deregister(Path pluginPath) {
    LOGGER.info(String.format("Deregistering: %s", pluginPath.toString()));

    if (!this.registrations.containsKey(pluginPath)) {
      LOGGER.fine("No registered classes, nothing to be done");
      return;
    }

    final var registration = this.registrations.get(pluginPath);
    final var extensions = registration.extensions();

    for (final var extensionType : extensions.keySet()) {
      if (!isSupportedExtensionType(extensionType)) {
        LOGGER.warning(
            String.format(
                "Skipping deregistration for unsupported extension type: %s",
                extensionType.getName()));

        continue;
      }

      final var extensionList = Jenkins.get().getExtensionList(extensionType);

      for (final var extension : extensions.get(extensionType)) {
        extensionList.remove(extension);

        LOGGER.info(
            String.format(
                "Removed %s extension: %s",
                extensionType.getName(), extension.getClass().getName()));
      }
    }

    try {
      registration.loader().close();
    } catch (IOException ex) {
      LOGGER.warning(
          String.format("Failed to close class loader for %s: %s", pluginPath, ex.getMessage()));
    }

    this.registrations.remove(pluginPath);
  }

  // NOTE(garrett): We must manually reify the type cast for the extension
  // point
  @SuppressWarnings("unchecked")
  void register(Path pluginPath) {
    LOGGER.info(String.format("Registering: %s", pluginPath.toString()));

    final var registration = this.createRegistrationFromJar(pluginPath.toFile());

    registration.ifPresent(
        pathRegistration -> {
          pathRegistration.extensions().entrySet().stream()
              .forEach(
                  entry -> {
                    final var extensionType = entry.getKey();

                    if (!isSupportedExtensionType(extensionType)) {
                      LOGGER.warning(
                          String.format(
                              "Skipping registration for unsupported extension type: %s",
                              extensionType.getName()));

                      return;
                    }

                    final var extensionList =
                        Jenkins.get().getExtensionList((Class<ExtensionPoint>) extensionType);

                    entry.getValue().stream()
                        .forEach(
                            instance -> {
                              // NOTE(garrett): The `add` method is deprecated but `add` with
                              // an index is not, though they do the same thing under the hood
                              extensionList.add(0, instance);

                              LOGGER.info(
                                  String.format(
                                      "%s extension: %s successfully installed into the instance",
                                      extensionType.getName(), instance.getClass().getName()));
                            });
                  });

          this.registrations.put(pluginPath, pathRegistration);
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
    final Set<Class<? extends ExtensionPoint>> extensionTypes = new HashSet<>();

    extensionTypes.addAll(currentRegistration.extensions().keySet());
    extensionTypes.addAll(newRegistration.extensions().keySet());

    for (final var extensionType : extensionTypes) {
      if (!isSupportedExtensionType(extensionType)) {
        LOGGER.warning(
            String.format(
                "Skipping reload for unsupported extension type: %s", extensionType.getName()));

        continue;
      }

      final var oldInstances =
          currentRegistration.extensions().getOrDefault(extensionType, List.of());
      final var newInstances = newRegistration.extensions().getOrDefault(extensionType, List.of());

      try {
        swapExtensionList(extensionType, oldInstances, newInstances);
      } catch (ReflectiveOperationException ex) {
        LOGGER.severe(
            String.format(
                "Failed to atomically reload extension type %s from %s: %s",
                extensionType.getName(), pluginPath.toString(), ex.getMessage()));

        return;
      }
    }

    try {
      currentRegistration.loader().close();
    } catch (IOException ex) {
      LOGGER.warning(
          String.format(
              "Failed to close class loader for previous registration at %s: %s",
              pluginPath.toString(), ex.getMessage()));
    }

    this.registrations.put(pluginPath, newRegistration);

    LOGGER.info(String.format("Re-registration complete: %s", pluginPath.toString()));
  }

  @SuppressWarnings("unchecked")
  private void swapExtensionList(
      Class<? extends ExtensionPoint> extensionType,
      List<? extends ExtensionPoint> oldInstances,
      List<? extends ExtensionPoint> newInstances)
      throws ReflectiveOperationException {
    final var extensionList =
        (ExtensionList<ExtensionPoint>) Jenkins.get().getExtensionList(extensionType);

    final Field extensionsField = ExtensionList.class.getDeclaredField("extensions");
    final Field legacyField = ExtensionList.class.getDeclaredField("legacyInstances");
    final Method sortMethod = ExtensionList.class.getDeclaredMethod("sort", List.class);

    extensionsField.setAccessible(true);
    legacyField.setAccessible(true);
    sortMethod.setAccessible(true);

    synchronized (extensionList) {
      final Set<ExtensionPoint> oldInstancesSet = new HashSet<>(oldInstances);
      final List<ExtensionComponent<ExtensionPoint>> workingComponents =
          new ArrayList<>(extensionList.getComponents());

      workingComponents.removeIf(component -> oldInstancesSet.contains(component.getInstance()));
      newInstances.stream()
          .map(instance -> new ExtensionComponent<>((ExtensionPoint) instance))
          .forEach(workingComponents::add);

      final var sortedComponents =
          (List<ExtensionComponent<ExtensionPoint>>)
              sortMethod.invoke(extensionList, workingComponents);

      extensionsField.set(extensionList, sortedComponents);

      final var legacyInstances =
          (CopyOnWriteArrayList<ExtensionComponent<ExtensionPoint>>) legacyField.get(extensionList);

      legacyInstances.removeIf(component -> oldInstancesSet.contains(component.getInstance()));
      newInstances.stream()
          .map(instance -> new ExtensionComponent<>((ExtensionPoint) instance))
          .forEach(legacyInstances::add);
    }

    fireExtensionListChangeListeners(extensionList);
  }

  // NOTE(garrett): While there's an API for this, it's restricted from being
  // called outside the class so we have to do it via reflection in order not to
  // cause the packaging to fail
  private void fireExtensionListChangeListeners(ExtensionList<?> extensionList)
      throws ReflectiveOperationException {
    final Method fireMethod = ExtensionList.class.getDeclaredMethod("fireOnChangeListeners");
    fireMethod.setAccessible(true);
    fireMethod.invoke(extensionList);
  }
}
