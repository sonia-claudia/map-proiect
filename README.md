[![CI](https://github.com/sonia-claudia/map-proiect/actions/workflows/ci.yml/badge.svg)](https://github.com/sonia-claudia/map-proiect/actions/workflows/ci.yml)

# Agenda de contacte

Proiect individual la disciplina Metode avansate de programare, anul universitar 2026-2027.

## Autor

- **Nume:** Suceava Sonia Claudia
- **Grupa:** 2.2
- **Marca:** LH715712
- **Tema:** 1 - Agenda de contacte
- **Limbaj:** C++20

## Descriere

Serviciu web care tine o agenda de contacte in memorie. Fiecare contact are nume, email,
telefon si categorie. Contactele se adauga, se cauta dupa nume, se filtreaza pe categorie,
se citesc dupa id, se sterg, iar o ruta de statistici numara contactele pe categorii.
Datele de intrare sunt validate, iar adresele de email sunt unice fara sa conteze
diferenta intre majuscule si minuscule.

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

Pentru a publica temporar aplicatia printr-un tunel Cloudflare, rulati pe rand:

```
docker pull ghcr.io/sonia-claudia/map-proiect:latest
docker run -d --name map -p 8080:8080 ghcr.io/sonia-claudia/map-proiect:latest
docker run --rm --network host cloudflare/cloudflared:latest tunnel --url http://localhost:8080
```

Comanda va afisa in terminal adresa publica a tunelului. Pastrati terminalul
deschis cat timp tunelul trebuie sa ramana activ.

Oprire: `Ctrl+C` pentru tunel, apoi `docker stop map && docker rm map`.

## Testare

```
cmake -B build -DBUILD_TESTS=ON
cmake --build build -j
./build/tests
```

## Rutele implementate

| Ruta | Metoda | Descriere |
|---|---|---|
| `/health` | GET | Starea serviciului |
| `/version` | GET | Versiunea si commit-ul din care a fost construita imaginea |
| `/` | GET | Pagina de prezentare |
| `/reset` | POST | Goleste datele din memorie |
| `/contacts` | POST | Adauga un contact (201), 400 la date invalide, 409 la email duplicat |
| `/contacts` | GET | Listeaza contactele, ordonate dupa nume; parametri optionali `category` si `q` |
| `/contacts/{id}` | GET | Intoarce un contact sau 404 |
| `/contacts/{id}` | DELETE | Sterge un contact (204) sau 404 |
| `/stats` | GET | Numarul total de contacte si numarul pe categorii |

## Decizii de implementare

- **Logica separata de server.** Validarea si agenda (`Store`) sunt in `src/app.hpp`, iar
  `src/main.cpp` doar leaga rutele. Astfel logica se testeaza fara sa porneasca un server.
- **Email normalizat doar la comparatie.** Unicitatea se verifica pe forma cu litere mici,
  dar in agenda se pastreaza si se intoarce emailul exact cum l-a scris utilizatorul.
- **Lungimea numelui se numara in caractere, nu in octeti.** Numele cu diacritice
  (UTF-8) nu sunt respinse gresit la limita de 50 de caractere.
- **Imagine Docker in doua etape.** Compilarea ruleaza in imaginea `gcc`, iar imaginea finala
  contine doar binarul legat static peste `debian:bookworm-slim`.
