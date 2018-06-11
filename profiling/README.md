# Profiling

The following describes how to setup the framework for the profiling of the bitcoin client

## setup
1. For the profiling of the regular client checkout at commit bf3353de90598f08a68d966c50b57ceaeb5b5d96
   for profiling the modified client checkout the current master branch.
2. Apply enable_profiler.diff to enable the profiler
3. Compile with the following settings:
   ```
   export CXXFLAGS="-lprofiler -ltcmalloc"
   export CFLAGS=-lprofiler -ltcmalloc
   ./configure --without-gui --enable-debug --disable-wallet
   make
   ```
4. Prepopulate the ~/.bitcoin folder by copying the current chain from somewhere
5. Copy bitcoin.conf to ~/.bitcoin
6. Copy test.py to ~/
7. Install the ping_client. For the setup guide see ping_client

## run experiment
Run one of the following:
   ```
   ./proftest.sh btc_client             # This runs the profiler experiment
   ./stresstest.sh btc_client           # This measures the general resource usage
   ./toptest.sh btc_client              # This measures the per thread cpu usage
   ./1000proftest.sh btc_client/client  # This runs the profiler on the modified 
                                        # client while connecting to 10000 clients
   ./1000test.sh btc_client/client      # This runs measures the general resource 
                                        # usage for the mod. client while connected
                                        # to 10000 clients.
   ```

The results are in btc_client/client/testresults* or btc_client/testresults*

## analyse the results
Use the scripts in ./analyse to interpret the results. The scripts need manual configuration.
