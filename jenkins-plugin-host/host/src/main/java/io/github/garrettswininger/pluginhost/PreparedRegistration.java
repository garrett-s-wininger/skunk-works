package io.github.garrettswininger.pluginhost;

import java.net.URLClassLoader;
import java.util.List;
import java.util.Map;

record PreparedRegistration(
    byte[] digest, URLClassLoader loader, Map<Class<?>, List<?>> delegates) {}
