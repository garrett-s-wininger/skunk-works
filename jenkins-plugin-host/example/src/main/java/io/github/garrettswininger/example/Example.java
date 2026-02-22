package io.github.garrettswininger.example;

import hudson.model.ManagementLink;
import io.github.garrettswininger.hosting.DynamicPlugin;
import io.github.garrettswininger.hosting.Hosted;

@Hosted
public class Example extends DynamicPlugin<ManagementLink, Action> {
  public Example() {
    super(ManagementLink.class, Action.class);
  }
}
