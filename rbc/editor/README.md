# Editor

## Source

- include/RBCEditor
- shaders
- src
- styles
- rbc_editor.qrc

update shader
`qsb -o shaders/prebuilt/quad.vert.qsb shaders/quad.vert`
`qsb -o shaders/prebuilt/quad.frag.qsb shaders/quad.frag`

## HttpClient

- `GET /scene/state/`: returns current scene entities and transforms
- `GET /resources/all`: return all resource metadata
- `GET /resources/{id}`: return specific resource metadata
- `POST /editor/register`: registers new editor connection
- `POST /editor/heartbeat`: Editor keep-alive heartbeat

