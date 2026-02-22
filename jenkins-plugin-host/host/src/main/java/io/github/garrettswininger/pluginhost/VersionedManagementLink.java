package io.github.garrettswininger.pluginhost;

import hudson.model.ManagementLink;
import java.util.concurrent.atomic.AtomicReference;
import org.kohsuke.stapler.StaplerProxy;

class VersionedManagementLink extends ManagementLink implements StaplerProxy {
  private final AtomicReference<ManagementLink> delegate;

  VersionedManagementLink(ManagementLink initialDelegate) {
    this.delegate = new AtomicReference<>(initialDelegate);
  }

  void swap(ManagementLink newDelegate) {
    this.delegate.set(newDelegate);
  }

  private ManagementLink currentDelegate() {
    return this.delegate.get();
  }

  @Override
  public Object getTarget() {
    return currentDelegate();
  }

  @Override
  public String getIconFileName() {
    return currentDelegate().getIconFileName();
  }

  @Override
  public String getDisplayName() {
    return currentDelegate().getDisplayName();
  }

  @Override
  public String getDescription() {
    return currentDelegate().getDescription();
  }

  @Override
  public String getUrlName() {
    return currentDelegate().getUrlName();
  }

  @Override
  public boolean getRequiresConfirmation() {
    return currentDelegate().getRequiresConfirmation();
  }

  @Override
  public hudson.security.Permission getRequiredPermission() {
    return currentDelegate().getRequiredPermission();
  }

  @Override
  public boolean getRequiresPOST() {
    return currentDelegate().getRequiresPOST();
  }

  @Override
  public Category getCategory() {
    return currentDelegate().getCategory();
  }

  @Override
  public jenkins.management.Badge getBadge() {
    return currentDelegate().getBadge();
  }
}
