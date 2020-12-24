open Oni_Core;
open Exthost;

[@deriving show]
type command =
  | RenameSymbol
  | Cancel
  | Commit;

[@deriving show]
type msg =
  | Command(command)
  | InputText(Component_InputText.msg);

[@deriving show]
type provider = {
  handle: int,
  selector: DocumentSelector.t,
  supportsResolveInitialValues: bool,
};

[@deriving show]
type sessionState =
  | Inactive
  | Resolving({sessionId: int})
  | Resolved({
      sessionId: int, //location: RenameLocation.t,
      inputText: [@opaque] Component_InputText.model,
    })
  | Applying({
      sessionId: int,
      edit: [@opaque] WorkspaceEdit.t, });

[@deriving show]
type model = {
  nextSessionId: int,
  sessionState,
  providers: list(provider),
};

let initial = {nextSessionId: 0, sessionState: Inactive, providers: []};

let register = (~handle, ~selector, ~supportsResolveInitialValues, model) => {
  ...model,
  providers: [
    {handle, selector, supportsResolveInitialValues},
    ...model.providers,
  ],
};

let unregister = (~handle, model) => {
  ...model,
  providers:
    model.providers |> List.filter(provider => provider.handle != handle),
};

let isFocused = ({sessionState, _}) => {
  switch (sessionState) {
  | Inactive => false
  | Resolving(_) => false
  | Resolved(_) => true
  | Applying(_) => true
  };
};

let update = (msg, model) => {
  switch (msg) {
  | InputText(inputMsg) =>
    switch (model.sessionState) {
    | Resolved({inputText, _} as resolved) =>
      let (inputText', _) = Component_InputText.update(inputMsg, inputText);
      (
        {
          ...model,
          sessionState: Resolved({...resolved, inputText: inputText'}),
        },
        Outmsg.Nothing,
      );
    | _ => (model, Outmsg.Nothing)
    }
  | Command(Commit)
  | Command(Cancel) => ({...model, sessionState: Inactive}, Outmsg.Nothing)
  | Command(RenameSymbol) =>
    let sessionId = model.nextSessionId;
    (
      {
        ...model,
        nextSessionId: sessionId + 1,
        sessionState:
          Resolved({
            sessionId,
            inputText: Component_InputText.create(~placeholder="hi"),
          }),
      },
      Outmsg.Nothing,
    );
  };
};

module Commands = {
  open Feature_Commands.Schema;

  let rename =
    define(
      ~category="Language Support",
      ~title="Rename Symbol",
      "editor.action.rename",
      Command(RenameSymbol),
    );

  let cancel = define("editor.action.rename.cancel", Command(Cancel));

  let commit = define("editor.action.rename.commit", Command(Commit));
};

module ContextKeys = {
  open WhenExpr.ContextKeys.Schema;

  // TODO:
  let renameInputVisible = bool("renameInputVisible", isFocused);
};

module Keybindings = {
  open Feature_Input.Schema;

  let condition = "normalMode" |> WhenExpr.parse;

  let rename = {key: "<F2>", command: Commands.rename.id, condition};

  let activeCondition = "renameInputVisible" |> WhenExpr.parse;

  let cancel = {
    key: "<ESC>",
    command: Commands.cancel.id,
    condition: activeCondition,
  };
  let commit = {
    key: "<CR>",
    command: Commands.commit.id,
    condition: activeCondition,
  };
};

module Contributions = {
  let commands = Commands.[rename, cancel, commit];

  let contextKeys = [ContextKeys.renameInputVisible];

   let keybindings = Keybindings.[rename, cancel, commit];
};

module View = {
  open Revery;
  open Revery.UI;
  module Styles = {
    open Style;
    let color = Color.rgba(0., 0., 0., 0.75);
    let boxShadow = (x, y) => [
      position(`Absolute),
      top(y),
      left(x),
      width(100),
      height(100),
      backgroundColor(Colors.red),
      boxShadow(
        ~xOffset=4.,
        ~yOffset=4.,
        ~blurRadius=12.,
        ~spreadRadius=0.,
        ~color,
      ),
    ];
  };
  module Colors = Feature_Theme.Colors;
  let make =
      (
        ~theme: ColorTheme.Colors.t,
        ~model: model,
        ~font: UiFont.t,
        ~dispatch: msg => unit,
        (),
      ) => {
        prerr_endline ("!!RenameView: " ++ show_model(model));
    switch (model.sessionState) {
    | Resolved({inputText, _}) =>
      prerr_endline ("!!!RENDERING");
      <View style=Styles.boxShadow(100, 100)>
        <View
          style=Style.[
            width(400),
            backgroundColor(Colors.Menu.background.from(theme)),
            color(Colors.Menu.foreground.from(theme)),
          ]>
          <View style=Style.[padding(5)]>
            <Component_InputText.View
              prefix="="
              model=inputText
              theme
              isFocused=true
              fontFamily={font.family}
              fontSize=14.
              dispatch={msg => dispatch(InputText(msg))}
            />
          </View>
        </View>
      </View>
    | _ => React.empty
    };
  };
};
