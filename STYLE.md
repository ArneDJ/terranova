# Terranova C++ engine coding style

The style is mostly inspired by SerenityOS' coding style, Bjarne Soustrup's C++ Core Guidelines and K&R style.

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

Use CamelCase for a class, struct and enum.
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

Enum members should use CamelCase.

Constant values that aren't arguments should always be written in caps with the words seperated by an underscore.

Namespaces should be named after the directory the file is located in.

```cpp
// src/physics/ragdoll.h

namespace physx {

class Ragdoll {
    ...
};

};
```

### Indentation
The K&R style (Kernighan & Ritchie Style) should always be used.

```cpp
int main(int argc, char *argv[])
{
    ...
    while (x == y) {
        something();
        somethingelse();

        if (some_error) {
            do_correct();
        } else {
            continue_as_usual();
        }
    }

    return 0;
}
```

### Pointers and References

When passing around pointers and references they should always be read-only unless you want them to be edited.
Prefer to use references over pointers in function arguments.

### "using" Statements

In C++ implementation files, do not use "using" declarations of any kind to import names in the standard template library. Directly qualify the names at the point they're used instead.

###### Right:

```cpp
// File.cpp

std::swap(a, b);
c = std::numeric_limits<int>::max()
```

###### Wrong:

```cpp
// File.cpp

using std::swap;
swap(a, b);
```

###### Wrong:

```cpp
// File.cpp

using namespace std;
swap(a, b);
```
