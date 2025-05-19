# Farming Simulator
A charming 3D farming simulation game where you manage animals to build your dream farm. Take care of various animals, manage resources, and grow your farming empire!

## 🕹️ Demo below
No time or can't run the game? No problem; just watch the demo below to see what are you missing
https://github.com/user-attachments/assets/10c60569-b135-4448-8348-90af9c03862f

## 🎮 Game Features

### 🐾 Animals
- **Available Animals:**
  - 🐔 Chickens: Your source of fresh eggs
  - 🐷 Pigs: Produce quality steak
  - 🐮 Cows: Provide fresh milk
  - Decorative animals:
    - 🐎 Horses
    - 🐱 Cats
    - 🐕 Dogs

### 🍽️ Animal Care System
#### Hunger Management
- Animals have hunger levels ranging from 0-100%
- Hunger depletes automatically:
  - Chickens: 5% every 10 seconds
  - Pigs: 8% every 10 seconds
  - Cows: 8% every 10 seconds
- Each animal requires 1 food unit for feeding
- Animals must be fed regularly to maintain productivity

#### Production Mechanics
- **Production Requirements:**
  - Animals need >20% hunger level to produce
  - Products must be collected manually
  - One product per animal when ready
- **Production Times:**
  - 🥚 Eggs: 30 seconds per chicken
  - 🥩 Steak: 120 seconds per pig
  - 🥛 Milk: 60 seconds per cow

### 💰 Economy
#### Starting Resources
- 100 coins
- 200 food units in barn

#### Pricing
- **Buy Prices:**
  - Food: 5 coins
  - Chicken: 20 coins
  - Pig: 50 coins
  - Cow: 100 coins
  - Farmhouse: 1000 coins
- **Sell Prices:**
  - Eggs: 2 coins each
  - Milk: 8 coins each
  - Steak: 15 coins each

### 🏪 Storage Systems
#### Barn Storage
- Central storage facility
- Stores:
  - Food supplies
  - Eggs
  - Milk
  - Steak
- Transfer items between inventory and barn

#### Player Inventory
- Single-slot system
- Can hold one type of item at a time
- Must be empty to collect new items

### 🏡 Buildings & Infrastructure
- **Core Buildings:**
  - Barn: Resource storage
  - Bank: Trading center
  - Animal enclosures
  - Farmhouse (purchasable upgrade)
- **Farm Layout:**
  - Custom road system
  - Decorative plants
  - Landscaping options

### 🌍 Environment
- **Dynamic World:**
  - Chunk-based terrain system
  - Day/night cycle
  - Atmospheric cloud system
- **Decorative Elements:**
  - Trees
  - Grass patches
  - Various flower types
  - Flowering bushes

### 🎯 Controls & Interaction
- **Navigation:**
  - Full 3D camera system
  - Proximity-based interaction system
- **Interaction Zones:**
  - Animal care areas
  - Building access points
  - Storage management
- **Menu System:**
  - Resource management
  - Trading interface
  - Animal care options

## 🎯 Game Objectives
1. Build and expand your farm
2. Manage animal welfare
3. Optimize production cycles
4. Generate profit through trading
5. Upgrade facilities
6. Create an efficient farm layout

## 💡 Tips for Success
1. Keep your animals well-fed for continuous production
2. Monitor hunger levels regularly
3. Collect products as soon as they're ready
4. Use the barn for efficient storage
5. Plan your farm layout for easy access
6. Balance your investments between animals and infrastructure

## 🔄 Game Loop
1. Feed and care for animals
2. Monitor production timers
3. Collect animal products
4. Store or sell products
5. Purchase upgrades and expansions
6. Repeat and optimize

## 🎮 Getting Started
1. Start with chickens - they're the most affordable
2. Maintain a good food supply
3. Establish a regular feeding routine
4. Save up for bigger animals
5. Expand gradually and sustainably

Enjoy building and managing your own virtual farm! 🚜🌾 

# 👉 How to build and run the game?

# 1. Choose your platform (available on Windows, MacOS and Linux) and follow the instructions below:

# VSCode Users (all platforms)
*Note* You must have a compiler toolchain installed in addition to vscode.

* Download the game
* Open the folder in VSCode
* Run the build task ( CTRL+SHIFT+B or F5 )
* You are good to go

# Windows Users
There are two compiler toolchains available for windows, MinGW-W64 (a free compiler using GCC), and Microsoft Visual Studio
## Using MinGW-W64
* Double click the `build-MinGW-W64.bat` file
* CD into the folder in your terminal
* run `make`

### Note on MinGW-64 versions
Make sure you have a modern version of MinGW-W64 (not mingw).
The best place to get it is from the W64devkit from
https://github.com/skeeto/w64devkit/releases
or the version installed with the raylib installer
#### If you have installed raylib from the installer
Make sure you have added the path

`C:\raylib\w64devkit\bin`

To your path environment variable so that the compiler that came with raylib can be found.
DO NOT INSTALL ANOTHER MinGW-W64 from another source such as msys2, you don't need it.

## Microsoft Visual Studio
* Run `build-VisualStudio2022.bat`
* double click the `.sln` file that is generated


# Linux
* CD into the build folder
* run `./premake5 gmake2`
* CD back to the root
* run `make`
* you are good to go

# MacOS
* CD into the build folder
* run `./premake5.osx gmake2`
* CD back to the root
* run `make`
* you are good to go

# 2. Run the game
* The executable is in `bin/Debug` folder with `farming-simulator` name 

