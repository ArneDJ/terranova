# Terranova C++ engine coding style

The style is mostly inspired by SerenityOS' coding style and Bjarne Soustrup's C++ Core Guidelines.

### Names

Keep variable names laconic and clear.
Ideally a variable name should only consist out of one word, if the variable consists of multiple words use snake\_case.
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

Private data members should be prefixed by "m\_".

###### Right:

```cpp
class String {
public:
    ...

private:
    int m_length { 0 };
};
```

###### Wrong:

```cpp
class String {
public:
    ...

    int length { 0 };
};
```
Precede setters with the word "set". Use bare words for getters. Setter and getter names should match the names of the variables being set/gotten.

###### Right:

```cpp
void set_count(int); // Sets m_count.
int count() const; // Returns m_count.
```

###### Wrong:

```cpp
void set_count(int); // Sets m_the_count.
int get_count() const; // Returns m_the_count.
```
Precede getters that return values through out arguments with the word "get".

###### Right:

```cpp
void get_filename_and_inode_id(String&, InodeIdentifier&) const;
```

###### Wrong:

```cpp
void filename_and_inode_id(String&, InodeIdentifier&) const;
```

Use descriptive verbs in function names.

###### Right:

```cpp
bool convert_to_ascii(short*, size_t);
```

###### Wrong:

```cpp
bool to_ascii(short*, size_t);
```

Leave meaningless variable names out of function declarations. A good rule of thumb is if the parameter type name contains the parameter name (without trailing numbers or pluralization), then the parameter name isn't needed. Usually, there should be a parameter name for bools, strings, and numerical types.

###### Right:

```cpp
void set_count(int);

void do_something(Context*);
```

###### Wrong:

```cpp
void set_count(int count);

void do_something(Context* context);
```
