package io.github.garrettswininger.pluginhost;

import hudson.ExtensionPoint;
import hudson.model.ManagementLink;
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
import java.util.concurrent.atomic.AtomicReference;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.logging.Logger;
import jenkins.model.Jenkins;

record PreparedManagementLinkRegistration(
    byte[] digest, URLClassLoader loader, List<ManagementLink> delegates) {}

record HostedRegistration(
    byte[] digest, URLClassLoader loader, List<VersionedManagementLink> links) {}

class VersionedManagementLink extends ManagementLink {
  private final AtomicReference<ManagementLink> delegate;

  VersionedManagementLink(ManagementLink initialDelegate) {
    this.delegate = new AtomicReference<>(initialDelegate);
  }

  void swap(ManagementLink newDelegate) {
    this.delegate.set(newDelegate);
  }

  private ManagementLink currentDelegate() {
    return this.delegate.get();
  }

  @Override
  public String getIconFileName() {
    return currentDelegate().getIconFileName();
  }

  @Override
  public String getDisplayName() {
    return currentDelegate().getDisplayName();
  }

  @Override
  public String getDescription() {
    return currentDelegate().getDescription();
  }

  @Override
  public String getUrlName() {
    return currentDelegate().getUrlName();
  }

  @Override
  public boolean getRequiresConfirmation() {
    return currentDelegate().getRequiresConfirmation();
  }

  @Override
  public hudson.security.Permission getRequiredPermission() {
    return currentDelegate().getRequiredPermission();
  }

  @Override
  public boolean getRequiresPOST() {
    return currentDelegate().getRequiresPOST();
  }

  @Override
  public Category getCategory() {
    return currentDelegate().getCategory();
  }

  @Override
  public jenkins.management.Badge getBadge() {
    return currentDelegate().getBadge();
  }
}

class AutoloadRegistry {
  private static final Logger LOGGER = Logger.getLogger(AutoloadRegistry.class.getName());

  private final Map<Path, HostedRegistration> registrations;

  AutoloadRegistry() {
    this.registrations = new HashMap<>();
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
  private Optional<PreparedManagementLinkRegistration> createRegistrationFromJar(File file) {
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

    final List<ManagementLink> delegates = new ArrayList<>();

    try (var jar = new JarFile(file)) {
      jar.stream()
          .filter(entry -> entry.getName().endsWith(".class"))
          .map(entry -> entryToPlugin(loader, entry))
          .forEach(
              entry -> {
                entry.ifPresent(
                    plugin -> {
                      if (!ManagementLink.class.isAssignableFrom(plugin.extension)) {
                        LOGGER.fine(
                            String.format(
                                "Skipping unsupported extension type for hosted loading: %s",
                                plugin.extension.getName()));
                        return;
                      }

                      final ExtensionPoint instance;

                      try {
                        instance = plugin.getInstance();
                      } catch (Exception ex) {
                        LOGGER.warning(
                            String.format(
                                "Failed to instantiate extension (%s): %s",
                                plugin.implementation.getName(), ex.getMessage()));

                        return;
                      }

                      if (!(instance instanceof ManagementLink)) {
                        LOGGER.warning(
                            String.format(
                                "Skipping extension due to incompatible instance type: %s",
                                instance.getClass().getName()));
                        return;
                      }

                      delegates.add((ManagementLink) instance);
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

    if (delegates.isEmpty()) {
      LOGGER.info(String.format("No ManagementLink extensions found in %s", file.getPath()));

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

    return Optional.of(
        new PreparedManagementLinkRegistration(calculateDigest(file.toPath()), loader, delegates));
  }

  void deregister(Path pluginPath) {
    LOGGER.info(String.format("Deregistering: %s", pluginPath.toString()));

    final var registration = this.registrations.get(pluginPath);

    if (registration == null) {
      LOGGER.fine("No registered classes, nothing to be done");
      return;
    }

    final var extensionList = Jenkins.get().getExtensionList(ManagementLink.class);

    for (final var link : registration.links()) {
      extensionList.remove(link);

      LOGGER.info(String.format("Removed %s extension", link.getClass().getName()));
    }

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
          final var extensionList = Jenkins.get().getExtensionList(ManagementLink.class);
          final List<VersionedManagementLink> links = new ArrayList<>();

          loadedRegistration
              .delegates()
              .forEach(
                  delegate -> {
                    final var versionedLink = new VersionedManagementLink(delegate);
                    extensionList.add(0, versionedLink);
                    links.add(versionedLink);

                    LOGGER.info(
                        String.format(
                            "%s extension successfully installed into the instance",
                            delegate.getClass().getName()));
                  });

          this.registrations.put(
              pluginPath,
              new HostedRegistration(
                  loadedRegistration.digest(), loadedRegistration.loader(), links));
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

    if (newRegistration.delegates().size() != currentRegistration.links().size()) {
      LOGGER.warning(
          String.format(
              "Cannot atomically reload %s because extension count changed (%d => %d)",
              pluginPath, currentRegistration.links().size(), newRegistration.delegates().size()));

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

    for (int i = 0; i < currentRegistration.links().size(); i++) {
      currentRegistration.links().get(i).swap(newRegistration.delegates().get(i));
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
            newRegistration.digest(), newRegistration.loader(), currentRegistration.links()));

    LOGGER.info(String.format("Re-registration complete: %s", pluginPath.toString()));
  }
}
