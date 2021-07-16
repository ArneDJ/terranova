# Terranova C++ engine coding style

The style is mostly inspired by SerenityOS' coding style and Bjarne Soustrup's C++ Core Guidelines.

### Variable Names
Keep variable names laconic and clear.
Ideally a variable name should only consists out of one word, if the variable consists of multiple words use snake\_case.
If your variable names are overly descriptive it most likely means your function is doing more than one thing.

###### Right:

```cpp
uint32_t age;
float speed;
size_t buffer_size;
```

###### Wrong:

```cpp
uint32_t Age
float redVehicleSpeedOnHighway;
size_t heightmap_image_buffer_size;
```

### Class and Struct Names
Use CamelCase for a class or struct.
Class and Struct names should be short but clear.
If the name of a class or struct is overly descriptive and consists of more than four words you're probably doing something wrong.

###### Right:

```cpp
struct Entity;
class Ragdoll;
class BotManager;
```

###### Wrong:

```cpp
struct entity;
class PlayerRagDollController;
class bot_manager;
```
