#ifndef _VIKINGS_H
#define _VIKINGS_H

// Viking object types
#define VIKING_BALEOG               0
#define VIKING_ERIK                 1
#define VIKING_OLAF                 2

// Object flags
#define OBJ_FLAG_FLIP_HORIZ         0x0040
#define OBJ_FLAG_FLIP_VERT          0x0080

// --------------------
// Viking object fields
// --------------------

//
// Sets the vikings reaction. The viking will react on the next game loop
// and then this field will be cleared.
//
// Shock reactions need to have damage assigned separately.
// Die reactions kill the viking instantly.
//
#define reaction                    field_30
#define VIKING_REACT_SHOCK_ZAP      0x01
#define VIKING_REACT_DIE_ZAP        0x02
#define VIKING_REACT_SHOCK_SKELETON 0x08
#define VIKING_REACT_DIE_FLATTEN    0x15
#define VIKING_REACT_BOUNCE         0x16

//
// Assign damage to a viking. Damage assigned to vikings is in multiples of 2
// (odd numbers will cause graphical corruption in the health bar). The damage
// will be applied to the viking on subsequent game loops.
//
#define assign_damage               field_32

#endif /* _VIKINGS_H */
