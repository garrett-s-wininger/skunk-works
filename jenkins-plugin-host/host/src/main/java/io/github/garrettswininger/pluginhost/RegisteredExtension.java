package io.github.garrettswininger.pluginhost;

import hudson.ExtensionPoint;
import java.util.List;

record RegisteredExtension(
    ExtensionAdapter<?, ?> adapter, List<? extends ExtensionPoint> wrappers) {}
