package io.github.garrettswininger.hosting;

import hudson.ExtensionPoint;
import jenkins.model.Jenkins;

public interface Extension<T extends ExtensionPoint> extends Hostable, Proxy<T> {
  @Override
  default void register() {
    Jenkins.get().getExtensionList(getExtensionPoint()).add(0, getImplementation());
  }
  ;

  @Override
  default void deregister() {
    Jenkins.get().getExtensionList(getExtensionPoint()).remove(getImplementation());
  }
  ;

  @Override
  default void reload() {
    throw new UnsupportedOperationException("Not supported");
  }
  ;

  Class<T> getExtensionPoint();

  T getImplementation();
}
