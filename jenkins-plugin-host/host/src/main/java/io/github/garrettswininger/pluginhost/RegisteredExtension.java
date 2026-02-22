package io.github.garrettswininger.pluginhost;

import java.util.List;

record RegisteredExtension(ExtensionAdapter<?, ?> adapter, List<?> wrappers) {}
