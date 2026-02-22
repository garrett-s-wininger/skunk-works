package io.github.garrettswininger.pluginhost;

import hudson.ExtensionPoint;
import java.net.URLClassLoader;
import java.util.Map;

record HostedRegistration(
    byte[] digest,
    URLClassLoader loader,
    Map<Class<? extends ExtensionPoint>, RegisteredExtension> extensions) {}
