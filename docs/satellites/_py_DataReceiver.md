<!-- markdownlint-disable MD041 -->
### Parameters inherited from `DataReceiver`

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `_file_name_pattern` | String | Pattern used to construct the filename for output files. This is interpreted as Python f-string. | `run_{run_identifier}_{date}.h5` |
| `_output_path` | String | Output directory the data files will be stored in. Interpreted as path which can be absolute or relative to the current directory. | `data` |
