# server

prijima:
- otazku na glob. stav

odosiela:
- pri kazdom update cely stav (mvp, neskor mozno iba updaty)

# client

prijima:
- glob.stav(resp iba update) -> updatne 

odosiela:
- updaty
  - zmena polohy cursoru
  - zmena znaku
- prikazy
  - odpoj

# protokol

client -> server posiela message dlzky 2. podla prveho znaku:
- `W`: (write) nasleduje char co ma napisat
- `S`: (special) nasleduje konkretna instrukcia:
  - `U`: up
  - `D`: down
  - `L`: left
  - `R`: right
  - `H`: home
  - `E`: end
  - `B`: break line
  - `X`: delete
- `DD`: (document) requests full document

