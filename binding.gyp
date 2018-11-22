{
  "targets": [
    {
      "include_dirs": [
        "<!(node -e \"require('nan')\")"
      ],
      "target_name": "serialism",
      "sources": [
        "src/native.cxx"
      ]
    }
  ]
}
