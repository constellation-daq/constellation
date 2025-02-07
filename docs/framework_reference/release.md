# Release Process

Here the release process for Constellation is described.

## Updating the Version and Code Name

Constellation roughly follows [semantic versioning](https://semver.org/), i.e. `MAJOR.FEATURE.PATCH`. The major version
should only be increased when incompatible changes are introduced, the feature version only when new features are added.

```{note}
Currently, Constellation is in a draft state at major version `0` and without patch releases. Incompatible changes may be
introduced in any release until major version `1`.
```

Each major and feature release of Constellation has a code name, chosen from the [IAU designated constellations](https://en.wikipedia.org/wiki/IAU_designated_constellations_by_area)
in order of increasing solid angle they take up in the sky.

The new version and code name need to be adjusted in the top-level `meson.build` file before the release.

## Publishing the Release Notes

The release notes need to be created in the [website repository](https://gitlab.desy.de/constellation/constellation.pages.desy.de).

```{note}
The release notes should be published before the release changes are merged into the main repository.
```

## Updating the Metadata

The metadata file located in `docs/etc` needs to be updated with a new release entry, which might look like:

```xml
<release version="0.3" date="2025-01-17">
  <url type="details">https://constellation.pages.desy.de/news/2025-01-17-Constellation-Release-0.3.html</url>
  <description>
    <p>Release of Constellation Sagitta</p>
  </description>
</release>
```

Note that the URL with the release notes needs to be updated to the published release notes.

## Creating the Release

The release on GitLab is created when an annotated tag (`git tag -s` or `git tag -m`) is pushed. The tag has to follow the
`vMAJOR.FEATURE.PATCH` naming scheme.

After the tag pipelines finished and the release is created on GitLab, the release should be edited to add the URL to the
release notes as release asset.

## Releasing on Flathub

The new release should be pushed to the [Flathub repository](https://github.com/flathub/de.desy.constellation).
This is done by updating the URL in the manifest to the full source code from the GitLab release assets.
