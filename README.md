[![CI](https://github.com/sonia-claudia/map-proiect/actions/workflows/ci.yml/badge.svg)](https://github.com/sonia-claudia/map-proiect/actions/workflows/ci.yml)

# [Titlul proiectului]

Proiect individual la disciplina Metode avansate de programare, anul universitar 2026-2027.

## Autor

- **Nume:** Suceava Sonia Claudia
- **Grupa:** 2.2
- **Marca:** LH715712
- **Tema:** 1 - Agenda de contacte

## Descriere

[Doua-trei propozitii despre ce face aplicatia si ce problema rezolva.]

## Tehnologii

C++20 cu cpp-httplib si nlohmann/json

## Rulare

```
docker build -t map-proiect .
docker run -d -p 8080:8080 --name map map-proiect
```

Aplicatia asculta pe portul 8080. Verificati:

```
curl http://localhost:8080/health
curl http://localhost:8080/version
```

## Expunere prin Cloudflare Tunnel

Pentru a publica temporar aplicatia printr-un tunel Cloudflare, porniti mai intai
containerul aplicatiei, apoi rulati:

```
docker pull ghcr.io/sonia-claudia/map-proiect:latest
docker run --rm --network host cloudflare/cloudflared:latest tunnel --url http://localhost:8080
```

Comanda va afisa in terminal adresa publica a tunelului. Pastrati terminalul
deschis cat timp tunelul trebuie sa ramana activ.

## Testare

```
cmake -B build -DBUILD_TESTS=ON
cmake --build build -j
./build/tests
test
```

## Rutele implementate

| Ruta | Metoda | Descriere |
|---|---|---|
| `/health` | GET | Starea serviciului |
| `/version` | GET | Versiunea si commit-ul din care a fost construita imaginea |
| `/` | GET | Pagina de prezentare |
| `/reset` | POST | Goleste datele din memorie |
| [ruta temei] | [metoda] | [descriere] |

## Decizii de implementare

[Doua-trei decizii tehnice pe care le-ati luat si motivul fiecareia.]
