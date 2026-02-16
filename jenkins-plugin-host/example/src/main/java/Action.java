import hudson.model.ManagementLink;

public class Action extends ManagementLink {
  @Override
  public String getDescription() {
    return "This is a test of dynamic attach";
  }

  @Override
  public String getDisplayName() {
    return "My Custom Action";
  }

  @Override
  public String getIconFileName() {
    return "symbol-star";
  }

  @Override
  public String getUrlName() {
    return "test";
  }
}
