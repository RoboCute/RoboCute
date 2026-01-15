# Editor Scene Service

对于编辑场景的Service，最重要的Service之一

- getEntity
- getAllEngities
- addEntity
- selectEntity
- clearSelection
- selectedEntitieIds
- isDirty
- markClean

Signal
- entityAdded
- entityRemoved
- entitySelected
- sceneChanged

NodeGraph


## ISceneService

SceneService的基本接口，后续每一轮更新迭代时候可以使用SceneServiceV2等实现类来继承

