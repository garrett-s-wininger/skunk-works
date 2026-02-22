package io.github.garrettswininger.pluginhost;

import java.net.URLClassLoader;
import java.util.Map;

record HostedRegistration(
    byte[] digest, URLClassLoader loader, Map<Class<?>, RegisteredExtension> extensions) {}
