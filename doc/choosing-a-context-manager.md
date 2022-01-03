-----
Title: Choosing a Context Manager Implementation
-----

# Choosing a Context Manager Implementation

Ususally it is fine to trust the auto-detection. If the operating system is not Linux,
there is only one choice anyway.

For Linux, four different implementations exist:

 - A basic polling implementation, the fall-back if nothing else works.
 - A Netlink-based implementation
 - Using NetworkManager to identify available network interfaces
 - Using Connman to identify the available interfaces
 - An Android-specific implementation
 
With the exception of Android, It is generally recommended to use the Netlink-based implementation.
It should co-exist with any other network management implementation.

