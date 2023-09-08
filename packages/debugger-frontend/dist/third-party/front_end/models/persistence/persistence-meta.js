import*as e from"../../core/common/common.js";import*as r from"../../core/i18n/i18n.js";import*as t from"../../core/sdk/sdk.js";import*as s from"../../ui/legacy/legacy.js";import*as o from"../workspace/workspace.js";const i={workspace:"Workspace",showWorkspace:"Show Workspace",enableLocalOverrides:"Enable Local Overrides",interception:"interception",override:"override",network:"network",rewrite:"rewrite",request:"request",enableOverrideNetworkRequests:"Enable override network requests",disableOverrideNetworkRequests:"Disable override network requests"},n=r.i18n.registerUIStrings("models/persistence/persistence-meta.ts",i),a=r.i18n.getLazilyComputedLocalizedString.bind(void 0,n);let c;async function d(){return c||(c=await import("./persistence.js")),c}s.ViewManager.registerViewExtension({location:"settings-view",id:"workspace",title:a(i.workspace),commandPrompt:a(i.showWorkspace),order:1,loadView:async()=>(await d()).WorkspaceSettingsTab.WorkspaceSettingsTab.instance()}),e.Settings.registerSettingExtension({category:e.Settings.SettingCategory.PERSISTENCE,title:a(i.enableLocalOverrides),settingName:"persistenceNetworkOverridesEnabled",settingType:e.Settings.SettingType.BOOLEAN,defaultValue:!1,tags:[a(i.interception),a(i.override),a(i.network),a(i.rewrite),a(i.request)],options:[{value:!0,title:a(i.enableOverrideNetworkRequests)},{value:!1,title:a(i.disableOverrideNetworkRequests)}]}),s.ContextMenu.registerProvider({contextTypes:()=>[o.UISourceCode.UISourceCode,t.Resource.Resource,t.NetworkRequest.NetworkRequest],loadProvider:async()=>(await d()).PersistenceActions.ContextMenuProvider.instance(),experiment:void 0});