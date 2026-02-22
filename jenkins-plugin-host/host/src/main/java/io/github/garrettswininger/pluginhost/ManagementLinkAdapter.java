package io.github.garrettswininger.pluginhost;

import hudson.model.ManagementLink;
import java.util.ArrayList;
import java.util.List;
import jenkins.model.Jenkins;

class ManagementLinkAdapter implements ExtensionAdapter<ManagementLink, VersionedManagementLink> {
  @Override
  public Class<ManagementLink> extensionType() {
    return ManagementLink.class;
  }

  @Override
  public List<VersionedManagementLink> registerStable(List<ManagementLink> delegates) {
    final var extensionList = Jenkins.get().getExtensionList(ManagementLink.class);
    final List<VersionedManagementLink> wrappers = new ArrayList<>();

    delegates.forEach(
        delegate -> {
          final var wrapper = new VersionedManagementLink(delegate);
          extensionList.add(0, wrapper);
          wrappers.add(wrapper);
        });

    return wrappers;
  }

  @Override
  public void swap(List<VersionedManagementLink> wrappers, List<ManagementLink> delegates) {
    if (wrappers.size() != delegates.size()) {
      throw new IllegalStateException(
          String.format(
              "Wrapper/delegate count mismatch for ManagementLink reload (%d != %d)",
              wrappers.size(), delegates.size()));
    }

    for (int i = 0; i < wrappers.size(); i++) {
      wrappers.get(i).swap(delegates.get(i));
    }
  }

  @Override
  public void deregister(List<VersionedManagementLink> wrappers) {
    final var extensionList = Jenkins.get().getExtensionList(ManagementLink.class);

    wrappers.forEach(extensionList::remove);
  }
}
