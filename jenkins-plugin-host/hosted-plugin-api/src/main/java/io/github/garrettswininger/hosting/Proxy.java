package io.github.garrettswininger.hosting;

public interface Proxy<T> {
  void swap(T implementation);
}
