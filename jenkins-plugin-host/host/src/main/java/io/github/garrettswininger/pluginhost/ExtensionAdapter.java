package io.github.garrettswininger.pluginhost;

import java.util.List;

interface ExtensionAdapter<T, W extends T> {
  Class<T> extensionType();

  List<W> registerStable(List<T> delegates);

  void swap(List<W> wrappers, List<T> delegates);

  void deregister(List<W> wrappers);
}
