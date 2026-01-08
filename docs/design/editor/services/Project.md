# ProjectService

根据[Project结构文件](../../../design/ProjectStructure.md)更新

- Project Meta
- User Preference
- Node Graph (User-Defined/Third-party)
- Node Template Resource

ProjectPaths
- assets
- docs
- datasets
- pretrained
- intermediate

ProjectConfig
- defaultScene
- startupGraph
- backend
- resourceVersion
- extra

ProjectInfo
- name
- version
- rbcVersion
- author
- description
- createdAt
- paths
- config
- raw

ProjectOpenOptions
- loadUserPreferences
- loadEditorSession

## IProjectService

- isOpen
- projectRoot
- projectFilePath
- projectInfo