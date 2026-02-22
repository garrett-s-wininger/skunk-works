package io.github.garrettswininger.hosting;

import hudson.model.ManagementLink;
import java.util.concurrent.atomic.AtomicReference;

public class ManagementLinkProxy extends ManagementLink implements Extension<ManagementLink> {
  private final AtomicReference<ManagementLink> implementation;

  public ManagementLinkProxy(ManagementLink implementation) {
    this.implementation = new AtomicReference<>(implementation);
  }

  @Override
  public void swap(ManagementLink implementation) {
    this.implementation.set(implementation);
  }

  @Override
  public ManagementLink getImplementation() {
    return this.implementation.get();
  }

  @Override
  public Class<ManagementLink> getExtensionPoint() {
    return ManagementLink.class;
  }

  @Override
  public String getIconFileName() {
    return this.implementation.get().getIconFileName();
  }

  @Override
  public String getDisplayName() {
    return this.implementation.get().getDisplayName();
  }

  @Override
  public String getUrlName() {
    return this.implementation.get().getUrlName();
  }

  @Override
  public String getDescription() {
    return this.implementation.get().getDescription();
  }
}
