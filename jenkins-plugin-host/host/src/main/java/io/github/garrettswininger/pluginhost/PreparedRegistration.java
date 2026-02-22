package io.github.garrettswininger.pluginhost;

import hudson.ExtensionPoint;
import java.net.URLClassLoader;
import java.util.List;
import java.util.Map;

record PreparedRegistration(
    byte[] digest,
    URLClassLoader loader,
    Map<Class<? extends ExtensionPoint>, List<? extends ExtensionPoint>> delegates) {}
