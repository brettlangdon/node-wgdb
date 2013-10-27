{
    "targets": [
        {
            "include_dirs": [
            ],
            "libraries": [
                "-L/usr/local/lib",
                "-lwgdb"
            ],
            "sources": [
                "src/utils.cc",
                "src/wgdb.cc",
                "src/main.cc"
            ],
            "target_name": "wgdb"
        }
    ]
}
