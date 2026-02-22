package io.github.garrettswininger.pluginhost;

import hudson.ExtensionPoint;
import java.util.List;

interface ExtensionAdapter<T extends ExtensionPoint, W extends T> {
  Class<T> extensionType();

  List<W> registerStable(List<T> delegates);

  void swap(List<W> wrappers, List<T> delegates);

  void deregister(List<W> wrappers);
}
