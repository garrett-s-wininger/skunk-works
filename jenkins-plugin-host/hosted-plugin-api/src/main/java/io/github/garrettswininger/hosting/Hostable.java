package io.github.garrettswininger.hosting;

public interface Hostable {
  void register();

  void deregister();

  void reload();
}
