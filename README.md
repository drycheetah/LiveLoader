# ModLoader
A simple native android mod loader that doesn't require permissions with JNI_OnLoad support

## How to use
1. Load the lib
2. Launch the game once to create a folder at ```Android/data/packageName/nativemods```
3. Close the game
4. From all native libs in the directory will be loaded

## Known Issues
In some games you may have to load the game twice before the nativemods folder is created
