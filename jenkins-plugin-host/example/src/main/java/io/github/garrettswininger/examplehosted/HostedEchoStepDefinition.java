package io.github.garrettswininger.examplehosted;

import hudson.model.TaskListener;
import java.util.Set;
import org.jenkinsci.plugins.workflow.steps.Step;
import org.jenkinsci.plugins.workflow.steps.StepContext;
import org.jenkinsci.plugins.workflow.steps.StepDescriptor;
import org.jenkinsci.plugins.workflow.steps.StepExecution;
import org.kohsuke.stapler.DataBoundConstructor;

public class HostedEchoStepDefinition extends Step {
  private static final long serialVersionUID = 1L;

  private final String message;

  @DataBoundConstructor
  public HostedEchoStepDefinition(String message) {
    this.message = message;
  }

  public String getMessage() {
    return this.message;
  }

  @Override
  public StepExecution start(StepContext context) {
    return new HostedEchoStepExecution(this, context);
  }

  public static Set<Class<?>> getRequiredContext() {
    return Set.of(TaskListener.class);
  }

  public static final class DescriptorImpl extends StepDescriptor {
    @Override
    public String getFunctionName() {
      return "hostedEcho";
    }

    @Override
    public String getDisplayName() {
      return "Hosted Echo Step";
    }

    @Override
    public boolean takesImplicitBlockArgument() {
      return false;
    }

    @Override
    public Set<? extends Class<?>> getRequiredContext() {
      return HostedEchoStepDefinition.getRequiredContext();
    }
  }
}
