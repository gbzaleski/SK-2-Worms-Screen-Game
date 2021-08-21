# SK-2-Worms-Screen-Game
C/C++ project divided into two parts - Server for hosting game and client for multiple players to participate in a match.
Project uses GUI provided by the University.
Client connects to GUI via TCP connection and to server via UDP.
Both client and server have implemented data check and validation measures.
After compiling project (make command can be used) there are to be used accordingly:
```
./screen-worms-client game_server_adress [-n player_name] [-p server_port] [-i gui_server_adress] [-r gui_server_port]
./screen-worms-server [-p port_number] [-s randomisation_seed] [-t turning_speed] [-v game_speed] [-w board_width] [-h board_height]
```

# Full project description in Polish language:
## 1. Gra robaki ekranowe
### 1.1. Zasady gry

Tegoroczne duże zadanie zaliczeniowe polega na napisaniu gry sieciowej. Gra rozgrywa się na prostokątnym ekranie. Uczestniczy w niej co najmniej dwóch graczy. Każdy z graczy steruje ruchem robaka. Robak je piksel, na którym się znajduje. Gra rozgrywa się w turach. W każdej turze robak może się przesunąć na inny piksel, pozostawiając ten, na którym był, całkowicie zjedzony. Robak porusza się w kierunku ustalonym przez gracza. Jeśli robak wejdzie na piksel właśnie jedzony lub już zjedzony albo wyjdzie poza ekran, to spada z ekranu, a gracz nim kierujący odpada z gry. Wygrywa ten gracz, którego robak pozostanie jako ostatni na ekranie. Szczegółowy algorytm robaka jest opisany poniżej.
### 1.2. Architektura rozwiązania

Na grę składają się trzy komponenty: serwer, klient, serwer obsługujący interfejs użytkownika. Należy zaimplementować serwer i klient. Aplikację implementującą serwer obsługujący graficzny interfejs użytkownika (ang. GUI) dostarczamy.

Serwer komunikuje się z klientami, zarządza stanem gry, odbiera od klientów informacje o wykonywanych ruchach oraz rozsyła klientom zmiany stanu gry. Serwer pamięta wszystkie zdarzenia dla bieżącej partii i przesyła je w razie potrzeby klientom.

Klient komunikuje się z serwerem gry oraz interfejsem użytkownika. Klient dba także o to, żeby interfejs użytkownika otrzymywał polecenia w kolejności zgodnej z przebiegiem partii oraz bez duplikatów.

Specyfikacje protokołów komunikacyjnych, rodzaje zdarzeń oraz formaty komunikatów i poleceń są opisane poniżej.
### 1.3. Parametry wywołania programów

Serwer:

./screen-worms-server [-p n] [-s n] [-t n] [-v n] [-w n] [-h n]


  * `-p n` – numer portu (domyślnie `2021`)
  * `-s n` – ziarno generatora liczb losowych (opisanego poniżej, domyślnie
    wartość uzyskana przez wywołanie `time(NULL)`)
  * `-t n` – liczba całkowita wyznaczająca szybkość skrętu
    (parametr `TURNING_SPEED`, domyślnie `6`)
  * `-v n` – liczba całkowita wyznaczająca szybkość gry
    (parametr `ROUNDS_PER_SEC` w opisie protokołu, domyślnie `50`)
  * `-w n` – szerokość planszy w pikselach (domyślnie `640`)
  * `-h n` – wysokość planszy w pikselach (domyślnie `480`)

Klient:

    ./screen-worms-client game_server [-n player_name] [-p n] [-i gui_server] [-r n]

    game_server – adres (IPv4 lub IPv6) lub nazwa serwera gry
    -n player_name – nazwa gracza, zgodna z opisanymi niżej wymaganiami
    -p n – port serwera gry (domyślne 2021)
    -i gui_server – adres (IPv4 lub IPv6) lub nazwa serwera obsługującego interfejs użytkownika (domyślnie localhost)
    -r n – port serwera obsługującego interfejs użytkownika (domyślnie 20210)

Do parsowania parametrów linii komend można użyć funkcji getopt z biblioteki standardowej: https://linux.die.net/man/3/getopt.
## 2. Protokół komunikacyjny pomiędzy klientem a serwerem

Wymiana danych odbywa się po UDP. W datagramach przesyłane są dane binarne, zgodne z poniżej zdefiniowanymi formatami komunikatów. W komunikatach wszystkie liczby przesyłane są w sieciowej kolejności bajtów.
### 2.1. Komunikaty od klienta do serwera

Komunikat od klienta do serwera ma kolejno następujące pola:

    session_id: 8 bajtów, liczba bez znaku
    turn_direction: 1 bajt, liczba bez znaku, wartość 0 → prosto, wartość 1 → w prawo, wartość 2 → w lewo
    next_expected_event_no – 4 bajty, liczba bez znaku
    player_name: 0–20 znaków ASCII o wartościach z przedziału 33–126, w szczególności spacje nie są dozwolone

Klient wysyła taki datagram co 30 ms.

Komunikacja odbywa się zawsze, nawet jeśli partia się jeszcze nie rozpoczęła lub już się zakończyła.

Pole turn_direction wskazuje, czy gracz chce skręcać (czy ma wciśniętą którąś ze strzałek w lewo lub prawo na klawiaturze).

Puste pole player_name oznacza, że klient nie ma zamiaru włączać się do gry, jednakże chętnie poobserwuje, co się dzieje na planszy.

Pole session_id jest takie samo dla wszystkich datagramów wysyłanych przez danego klienta. Klient przy uruchomieniu ustala session_id na bieżący czas wyrażony w mikrosekundach od 1970-01-01 00:00:00 +0000 (UTC).
### 2.2. Komunikaty od serwera do klienta

Komunikat od serwera do klienta ma kolejno następujące pola:

    game_id: 4 bajty, liczba bez znaku
    events: zmienna liczba rekordów, zgodnych z poniższą specyfikacją

Serwer wysyła taki komunikat natychmiast po odebraniu komunikatu od klienta. Wysyła zdarzenia o numerach, począwszy od odebranego next_expected_event_no aż do ostatniego dostępnego. Jeśli takich zdarzeń nie ma, serwer nic nie wysyła w odpowiedzi. Serwer także wysyła taki komunikat do wszystkich klientów po pojawieniu się nowego zdarzenia.

Maksymalny rozmiar pola danych datagramu UDP wysyłanego przez serwer wynosi 548 bajtów. Jeśli serwer potrzebuje wysłać więcej zdarzeń, niż może zmieścić w jednym datagramie UDP, wysyła je w kolejnych komunikatach. W tym przypadku wszystkie oprócz ostatniego muszą zawierać maksymalną liczbę zdarzeń możliwą do umieszczenia w pojedynczym datagramie.

Pole game_id służy do identyfikacji bieżącej partii w sytuacji, gdy do klienta mogą dochodzić opóźnione datagramy z uprzednio zakończonej partii.
### 2.3. Rekordy opisujące zdarzenia

Rekord opisujący zdarzenie ma następujący format:

    len: 4 bajty, liczba bez znaku, sumaryczna długość pól event_*
    event_no: 4 bajty, liczba bez znaku, dla każdej partii kolejne wartości, począwszy od zera
    event_type: 1 bajt
    event_data: zależy od typu, patrz opis poniżej
    crc32: 4 bajty, liczba bez znaku, suma kontrolna obejmująca pola od pola len do event_data włącznie, obliczona standardowym algorytmem CRC-32-IEEE

Możliwe rodzaje zdarzeń:

    NEW_GAME
        event_type: 0
        event_data:
            maxx: 4 bajty, szerokość planszy w pikselach, liczba bez znaku
            maxy: 4 bajty, wysokość planszy w pikselach, liczba bez znaku
            następnie lista nazw graczy zawierająca dla każdego z graczy player_name, jak w punkcie „2.1. Komunikaty od klienta do serwera”, oraz znak '\0'
    PIXEL
        event_type: 1
        event_data:
            player_number: 1 bajt
            x: 4 bajty, odcięta, liczba bez znaku
            y: 4 bajty, rzędna, liczba bez znaku
    PLAYER_ELIMINATED
        event_type: 2
        event_data:
            player_number: 1 bajt
    GAME_OVER
        event_type: 3
        event_data: brak

Wśród nazw graczy w zdarzeniu NEW_GAME nie umieszcza się pustych nazw obserwatorów.

Kolejność graczy w zdarzeniu NEW_GAME oraz ich numerację w zdarzeniach PIXEL i NEW_GAME ustala się, ustawiając alfabetycznie ich nazwy. Graczy numeruje się od zera.
### 2.4. Generator liczb losowych

Do wytwarzania wartości losowych należy użyć poniższego deterministycznego generatora liczb 32-bitowych. Kolejne wartości zwracane przez ten generator wyrażone są wzorem:

r_0 = seed
r_i = (r_{i-1} * 279410273) mod 4294967291

gdzie wartość seed jest 32-bitowa i jest przekazywana do serwera za pomocą parametru -s (domyślnie są to 32 młodsze bity wartości zwracanej przez wywołanie time(NULL)). W pierwszym wywołaniu generatora powinna zostać zwrócona wartość r_0 == seed.

Należy użyć dokładnie takiego generatora, żeby umożliwić automatyczne testowanie rozwiązania (uwaga na konieczność wykonywania pośrednich obliczeń na typie 64-bitowym).
### 2.5. Stan gry

Podczas partii serwer utrzymuje stan gry, w skład którego wchodzą m.in.:

    numer partii (game_id), wysyłany w każdym wychodzącym datagramie
    bieżące współrzędne robaka każdego z graczy (jako liczby zmiennoprzecinkowe o co najmniej podwójnej precyzji) oraz kierunek ruchu robaka
    zdarzenia wygenerowane od początku gry (patrz punkt „2.3. Rekordy opisujące zdarzenia” oraz dalej)
    zjedzone piksele planszy

Lewy górny róg planszy ma współrzędne (0, 0), odcięte rosną w prawo, a rzędne w dół. Kierunek ruchu jest wyrażony w stopniach, zgodnie z ruchem wskazówek zegara, a 0 oznacza kierunek w prawo.

Warto tu podkreślić, że bieżąca pozycja robaka jest obliczana i przechowywana w formacie zmiennoprzecinkowym. Przy konwersji pozycji zmiennoprzecinkowej na współrzędne piksela stosuje się zaokrąglanie w dół. Uznajemy, że pozycja po zaokrągleniu znajduje się na planszy, jeśli rzędna znajduje się w przedziale domkniętym [0, maxy - 1], a odcięta w przedziale domkniętym [0, maxx - 1].
### 2.6. Podłączanie i odłączanie graczy

Podłączenie nowego gracza może odbyć się w dowolnym momencie. Wystarczy, że serwer odbierze prawidłowy komunikat od nowego klienta. Jeśli nowy gracz podłączy się podczas partii, staje się jej obserwatorem, otrzymuje informacje o wszystkich zdarzeniach, które miały miejsce od początku partii. Do walki dołącza w kolejnej partii.

Jeśli podłączy się gracz, który w komunikatach przesyła puste pole player_name, to taki gracz nie walczy, ale może obserwować rozgrywane partie.

Brak komunikacji od gracza przez 2 sekundy skutkuje jego odłączeniem. Jeśli gra się już rozpoczęła, robak takiego gracza nie znika i nadal porusza się wg algorytmu z punktu „2.8. Przebieg partii”.

Klienty są identyfikowane za pomocą par (gniazdo, session_id), jednakże otrzymanie komunikatu z gniazda istniejącego klienta, aczkolwiek z większym niż dotychczasowe session_id, jest równoznaczne z odłączeniem istniejącego klienta i podłączeniem nowego. Komunikaty z mniejszym niż dotychczasowe session_id należy ignorować.

Pakiety otrzymane z nieznanego dotychczas gniazda, jednakże z nazwą podłączonego już klienta, są ignorowane.
### 2.7. Rozpoczęcie partii i zarządzanie podłączonymi klientami

Do rozpoczęcia partii potrzeba, aby wszyscy podłączeni gracze (o niepustej nazwie) nacisnęli strzałkę (przysłali wartość turn_direction różną od zera) oraz żeby tych graczy było co najmniej dwóch.

Stan gry jest inicjowany w następujący sposób (kolejność wywołań rand() ma znaczenie i należy użyć generatora z punktu „2.4. Generator liczb losowych”). Graczy inicjuje się w kolejności alfabetycznej ich nazw.

game_id = rand()
wygeneruj zdarzenie NEW_GAME
dla kolejnych graczy zainicjuj pozycję i kierunek ruchu ich robaków
  x_robaka_gracza = (rand() mod maxx) + 0.5
  y_robaka_gracza = (rand() mod maxy) + 0.5
  kierunek_robaka_gracza = rand() mod 360
  jeśli piksel zajmowany przez robaka jest jedzony, to
    wygeneruj zdarzenie PLAYER_ELIMINATED
  w przeciwnym razie
    wygeneruj zdarzenie PIXEL

A zatem z gry można odpaść już na starcie.
### 2.8. Przebieg partii

Partia składa się z tur. Tura trwa 1/ROUNDS_PER_SEC sekundy. Ruchy graczy wyznacza się w kolejności alfabetycznej ich nazw.

dla kolejnych graczy
  jeśli ostatni turn_direction == 1, to
    kierunek_robaka_gracza += TURNING_SPEED
  jeśli ostatni turn_direction == 2, to
    kierunek_robaka_gracza −= TURNING_SPEED
  przesuń robaka o 1 w bieżącym kierunku
  jeśli w wyniku przesunięcia robak nie zmienił piksela, to
    continue
  jeśli robak zmienił piksel na piksel jedzony lub już zjedzony, albo wyszedł poza planszę, to
    wygeneruj zdarzenie PLAYER_ELIMINATED
  w przeciwnym razie
    wygeneruj zdarzenie PIXEL

### 2.9. Zakończenie partii

Gdy na planszy zostanie tylko jeden robak, gra się kończy. Generowane jest zdarzenie GAME_OVER. Po zakończeniu partii serwer wciąż obsługuje komunikację z klientami. Jeśli w takiej sytuacji serwer otrzyma od każdego podłączonego klienta (o niepustej nazwie) co najmniej jeden komunikat z turn_direction różnym od zera, rozpoczyna kolejną partię. Klienty muszą radzić sobie z sytuacją, gdy po rozpoczęciu nowej gry będą dostawać jeszcze stare, opóźnione datagramy z poprzedniej gry.
## 3. Protokół komunikacyjny pomiędzy klientem a interfejsem użytkownika

Wymiana danych odbywa się po TCP. Komunikaty przesyłane są w formie tekstowej, każdy w osobnej linii. Liczby są reprezentowane dziesiętnie. Linia zakończona jest znakiem o kodzie ASCII 10. Jeśli linia zawiera kilka wartości, to wysyłający powinien te wartości oddzielić pojedynczą spacją i nie dołączać dodatkowych białych znaków na początku ani na końcu.

Odbiorca niepoprawnego komunikatu ignoruje go. Odebranie komunikatu nie jest potwierdzane żadną wiadomością zwrotną.

Serwer obsługujący interfejs użytkownika akceptuje następujące komunikaty:

    NEW_GAME maxx maxy player_name1 player_name2 …
    PIXEL x y player_name
    PLAYER_ELIMINATED player_name

Nazwa gracza to ciąg 1–20 znaków ASCII o wartościach z przedziału 33–126. Współrzędne x, y to liczby całkowite, odpowiednio od 0 do maxx − 1 lub maxy − 1. Lewy górny róg planszy ma współrzędne (0, 0), odcięte rosną w prawo, a rzędne w dół.

Serwer obsługujący interfejs użytkownika wysyła następujące komunikaty:

    LEFT_KEY_DOWN
    LEFT_KEY_UP
    RIGHT_KEY_DOWN
    RIGHT_KEY_UP

## 4. Ustalenia dodatkowe

Programy powinny umożliwiać komunikację zarówno przy użyciu IPv4, jak i IPv6.

W implementacji programów duże kolejki komunikatów, zdarzeń itp. powinny być alokowane dynamicznie.

Przy parsowaniu ciągów zdarzeń z datagramów przez klienta:

    pierwsze zdarzenie z niepoprawną sumą kontrolną powoduje zaprzestanie przetwarzania kolejnych w tym datagramie, ale poprzednie pozostają w mocy;
    rekord z poprawną sumą kontrolną, znanego typu, jednakże z bezsensownymi wartościami, powoduje zakończenie klienta z odpowiednim komunikatem i kodem wyjścia 1;
    pomija się zdarzenia z poprawną sumą kontrolną oraz nieznanym typem.

Program klienta w przypadku błędu połączenia z serwerem gry lub interfejsem użytkownika powinien się zakończyć z kodem wyjścia 1, uprzednio wypisawszy zrozumiały komunikat na standardowe wyjście błędów.

Program serwera powinien być odporny na sytuacje błędne, które dają szansę na kontynuowanie działania. Intencja jest taka, że serwer powinien móc być uruchomiony na stałe bez konieczności jego restartowania, np. w przypadku kłopotów komunikacyjnych, czasowej niedostępności sieci, zwykłych zmian jej konfiguracji itp.

Serwer nie musi obsługiwać więcej niż 25 podłączonych graczy jednocześnie. Dodatkowi gracze ponad limit nie mogą jednak przeszkadzać wcześniej podłączonym.

W serwerze opóźnienia w komunikacji z jakimś podzbiorem klientów nie mogą wpływać na jakość komunikacji z pozostałymi klientami. Analogicznie w kliencie opóźnienia w komunikacji z interfejsem użytkownika nie mogą wpływać na regularność wysyłania komunikatów do serwera gry. Patrz też: https://stackoverflow.com/questions/4165174/when-does-a-udp-sendto-block.

Czynności okresowe (tury oraz wysyłanie komunikatów od klienta do serwera) powinny być wykonywane w odstępach niezależnych od czasu przetwarzania danych, obciążenia komputera czy też obciążenia sieci. Implementacja, która np. robi sleep(20ms) nie spełnia tego warunku. Dopuszczalne są krótkofalowe odchyłki, ale długofalowo średni odstęp czynności musi być zgodny ze specyfikacją.

Na połączeniu klienta z serwerem obsługującym interfejs użytkownika powinien zostać wyłączony algorytm Nagle'a, aby zminimalizować opóźnienia transmisji.

W przypadku otrzymania niepoprawnych argumentów linii komend, programy powinny wypisywać stosowny komunikat na standardowe wyjście błędów i zwracać kod 1.

Należy przyjąć rozsądne ograniczenia na wartości parametrów, w szczególności rozsądne ograniczenie na maksymalny rozmiar planszy, rozsądne limity na wartości parametrów ROUNDS_PER_SEC i TURNING_SPEED, dopuszczać tylko dodatnie wartości parametru tam, gdzie zerowa lub ujemna wartość nie ma sensu.
