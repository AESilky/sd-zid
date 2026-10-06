## Managing the z80dasm Subtree Component

The core directory `src-rp/lib/z80dasm` is managed as a Git subtree pointing to the isolated `src` directory of the `z80util` repository. The remote pointer `external-z80util` is persistent.

### Initial Machine Setup (One-time per environment)

If checking out this repository on a new machine or environment, register the upstream remote tracker:

```bash
git remote add external-z80util git@github.com:AESilky/z80util.git
```

### Pulling Upstream Updates (Option A workflow)

To pull down the latest changes made to the standalone utility repository:

```bash
# 1. Fetch the remote commit trees
git fetch external-z80util

# 2. Merge changes directly into the nested layout
git subtree merge --prefix=src-rp/lib/z80dasm external-z80util/main --squash
```

### Pushing Changes Back Upstream

If edits are made locally inside `src-rp/lib/z80dasm` and need to be pushed back to the main utility codebase:

```bash
git subtree push --prefix=src-rp/lib/z80dasm external-z80util dasm-update
```

 *Note:* `dasm-update` is a development branch on the 'z80util' repository. It will be created if it doesn't exist.

 *Note:* Ensure modifications to the subtree folder are bundled into dedicated commits separate from changes to the main application
