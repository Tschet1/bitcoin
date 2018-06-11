# Ping Client

## setup
1. Checkout bitcoin repository at commit bf3353de90598f08a68d966c50b57ceaeb5b5d96 to ~/ping_machine
2. Apply pingMachine.diff
3. Compile with the following settings:
   ```
   ./configure --without-gui --enable-debug --disable-wallet
   make
   ```
4. Prepopulate the ~/.ping_data folder by copying the current chain from somewhere
