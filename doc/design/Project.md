# RBCProject

- project.rbc
- Content
- Assets
  - hash.rasset: the binary file format 
- Intermediate
  - .uipc
    - <uipc_execute_hash>
- logs.db: The Sqlite database for executing log

- systems.json
- scene.json
- Cube.obj
- Cube.001.obj
- anim.json

## scene.json

- meshes
  - Cube
    - matrix_world
    - constitution_type
    - is_fixed
    - enable_self_collision
  - Cube.001
    - matrix_world
    - constitution_type
    - is_fixed
    - enable_self_collision
- sim_frame_start
- sim_frame_end
- sim_dt

## systems.json

- features
  - core/affine_body_state_accessor
  - core/contact_system
- sim_systems []
  - engine_aware
  - name
  - strong_deps
  - valid
  - weak_deps

## anim.json

- name
  - slot_id
  - original_vs
  - anim []
    - 0
      - matrix_world
      - co