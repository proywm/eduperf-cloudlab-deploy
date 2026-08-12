from typing import List, Text, Optional

# In-file constants (action name strings), mirroring the originals.
ACTION_LISTEN_NAME = "action_listen"
ACTION_RESTART_NAME = "action_restart"
ACTION_SESSION_START_NAME = "action_session_start"
ACTION_DEFAULT_FALLBACK_NAME = "action_default_fallback"
ACTION_DEACTIVATE_FORM_NAME = "action_deactivate_form"
ACTION_REVERT_FALLBACK_EVENTS_NAME = "action_revert_fallback_events"
ACTION_DEFAULT_ASK_AFFIRMATION_NAME = "action_default_ask_affirmation"
ACTION_DEFAULT_ASK_REPHRASE_NAME = "action_default_ask_rephrase"
ACTION_BACK_NAME = "action_back"
RULE_SNIPPET_ACTION_NAME = "..."


class Action:
    def name(self) -> Text:
        raise NotImplementedError


class ActionListen(Action):
    def name(self) -> Text:
        return ACTION_LISTEN_NAME


class ActionRestart(Action):
    def name(self) -> Text:
        return ACTION_RESTART_NAME


class ActionSessionStart(Action):
    def name(self) -> Text:
        return ACTION_SESSION_START_NAME


class ActionDefaultFallback(Action):
    def name(self) -> Text:
        return ACTION_DEFAULT_FALLBACK_NAME


class ActionDeactivateForm(Action):
    def name(self) -> Text:
        return ACTION_DEACTIVATE_FORM_NAME


class ActionRevertFallbackEvents(Action):
    def name(self) -> Text:
        return ACTION_REVERT_FALLBACK_EVENTS_NAME


class ActionDefaultAskAffirmation(Action):
    def name(self) -> Text:
        return ACTION_DEFAULT_ASK_AFFIRMATION_NAME


class ActionDefaultAskRephrase(Action):
    def name(self) -> Text:
        return ACTION_DEFAULT_ASK_REPHRASE_NAME


class TwoStageFallbackAction(Action):
    def __init__(self, action_endpoint=None):
        self.action_endpoint = action_endpoint

    def name(self) -> Text:
        return "two_stage_fallback"


class ActionBack(Action):
    def name(self) -> Text:
        return ACTION_BACK_NAME


def default_actions(action_endpoint: Optional[object] = None) -> List["Action"]:
    """List default actions."""
    return [
        ActionListen(),
        ActionRestart(),
        ActionSessionStart(),
        ActionDefaultFallback(),
        ActionDeactivateForm(),
        ActionRevertFallbackEvents(),
        ActionDefaultAskAffirmation(),
        ActionDefaultAskRephrase(),
        TwoStageFallbackAction(action_endpoint),
        ActionBack(),
    ]


def default_action_names() -> List[Text]:
    """List default action names."""
    return [a.name() for a in default_actions()] + [RULE_SNIPPET_ACTION_NAME]


def combine_user_with_default_actions(user_actions: List[Text]) -> List[Text]:
    # remove all user actions that overwrite default actions
    unique_user_actions = [a for a in user_actions if a not in default_action_names()]
    return default_action_names() + unique_user_actions
