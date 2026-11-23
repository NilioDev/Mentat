# Jessica's powers

Jessica teaches Paul her powers in her talks: her lines with action 8 (see
[talking.md](talking.md)) let him reach his troops from further off. How
far is word `0x1176` of the state, compared with a troop's distance from
Paul (see [orders.md](orders.md#the-commands)); at 1 he reaches only the
troops at his place, and from 2 on he can contact troops from the map
(see [map.md](map.md#the-commands)).

Each such line:

- After the water of life (state byte `0x0A` bit 1, see
  [stilgar.md](stilgar.md)): charisma (byte `0x29`) goes up by `0x28`, and
  the reach becomes `0xFFE2`, everywhere.
- Otherwise, the first time (the reach is 1): charisma goes up by `0x0A`
  and the reach becomes `0x1E`.
- Otherwise the reach grows by `0x14`, kept to 16 bits.

Charisma goes up as when a place is taken (at most `0xC8`, and the troops'
spirits rise with each step of four, see [places_taken.md](places_taken.md)).
Then byte `0xD5` becomes 0 when the reach is 100 or more, else `0x80` less
the reach divided by 6.

Her lines with action 15 count one more at byte `0xF5`.

