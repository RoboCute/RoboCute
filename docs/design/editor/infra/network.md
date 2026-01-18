# Editor Network Infra

编辑器网络层抽象

## HttpClient

- set/get server url

Project Level

Node Graph Level
- fetchAllNodes
- fetchNodeDetails
- executeNodeGraph
- fetchExecutionOutputs
- healthCheck

Scene Level
- getSceneState
- getAllResources
- registerEditor
- sendHeartbeat
- sendEditorCommand
- getAnimations
- getAnimationData


signals
- connectionStatusChanged
- errorOccurred

impl
- sendGetRequest
- setPostRequest
- handleReplyError

