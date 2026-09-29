# ASGE Editor - Issues

- [x] I have tried to delete a child entity and I got the windows abort window.
      I do not know if it is important but I have created a Panel P1, then a child
      Panel P2 and then I clicked on `New Child` on P2. Then I created on the `Delete`
      button of this new child in the entity inspector window and the process
      just crashed.

      Root cause: `Registry::DestroyEntity` stripped the entity's components but
      never unlinked it from the `Hierarchy` tree, so its parent kept
      `m_FirstChild`/`m_LastChild` pointing at the dead entity and the next
      `ForEachChild` over that parent aborted. It wasn't specific to panels --
      the Inspector's `Delete` (and the tree's `Remove` on a non-root) hit it for
      any child. `DestroyEntity` now detaches the entity from its parent and
      siblings, and turns its own children into roots; `ForEachChild` stops at a
      dead link instead of aborting.

      Follow-up found along the way: a scene saved after that bug held corrupt
      links (an entity that was its own parent), and loading it hung the editor
      forever in `Registry::IsDisabled`'s walk up the parent chain. Scene load
      now runs `SanitizeHierarchy`, which rebuilds every link from the parent
      pointers -- dropping self-parents and cycles, keeping the stored sibling
      order where it's consistent -- so a corrupt scene loads instead of hanging.

- [x] Duplicate Entity does not work for Hierarchy ...or more importantly,
      in case it is inside a panel with a layout it is not applied for the
      duplicated entity.

      Root cause: `DuplicateEntity` copied every serializable component
      verbatim, `Hierarchy` included, so the copy claimed the original's
      parent, siblings and children without being in any of their lists. The
      parent's child list never contained the copy, so `UILayoutSystem` never
      laid it out. `Hierarchy` is no longer copied: the copy is attached to the
      original's parent (after its existing children, so it takes the panel's
      next layout slot), and the original's whole subtree is duplicated under
      it.