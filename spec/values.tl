test "cannot compare unrelated values":
    assert not try(null > 1)
    assert not try(null < 1)
