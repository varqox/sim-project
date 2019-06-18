# Co to jest paczka i jak to działa na simie?

Niewątpliwie, do każdego zadania jest jakaś treść. Jak submitujemy, widzimy statusy kodu na testach oraz wyniki, zużytą pamięć, czas wykonania kodu / czas maksymalny na danym teście. Czy są to jedyne rzeczy, które są potrzebne do stworzenia paczki?

Proces sprawdzania kodu użytkownika przebiega intuicyjnie. Gdy submitujemy kod, sim wyszukuje odpowiednią paczkę (w formacie zip), wypakowuje z niej testy, kompiluje rozwiązanie użytkownika i po kolei sprawdza poprawność działania kodu na testach - uruchamia kod, przekierowuje wejścia z pliku wejściowego testu do programu i zapisuje wyjście programu. Jeżeli kod przekroczył limit czasu/pamięci/naruszył zasady bezpieczeństwa, sim kończy sprawdzanie testu i uaktualnia status zgłoszenia. Jeżeli kod wykonał się poprawnie, sprawdza stworzone wyjście z wzorcowym wyjściem za pomocą "checkerki" - programu, który stwierdza poprawność pliku wyjściowego (zazwyczaj jest to po prostu sprawdzenie, czy zostały wypisane takie same liczby, jednak w niektórych zadaniach istnieje wiele poprawnych odpowiedzi i wtedy proste porównanie odpowiedzi użytkownika z wzorcową nie wystarcza).

# Pierwsze użycie programu sip

* Pobranie i zainstalowanie już skompilowanego sipa:
```sh
sh -c 'mkdir -p "$HOME/.local/bin" && curl https://sim.ugo.si/kit/sip -o "$HOME/.local/bin/sip" -z "$HOME/.local/bin/sip" && chmod +x "$HOME/.local/bin/sip"'
```

* Własnoręczna kompilacja sipa: Jest opisana w https://github.com/varqox/sip. Dla wygody, przenieś skompilowany program pod nazwą `sip` do folderu `~/.local/bin/` lub `/usr/local/bin/`, dzięki czemu możesz użyć polecenia `sip`.

Ustal jak będzie się nazywało twoje zadanie i jaki będzie jego tag/label (zazwyczaj trzyliterowe słowo złożone z małych liter alfabetu angielskiego). W przykładach będzie używany label `tag` oraz nazwa zadania `Nazwa Zadania`.

Przejdź do odpowiedniego folderu i wykonaj polecenie `sip init nazwa_zadania`.
Stworzy to główny folder paczki, w którym będą się znajdować wszystkie pliki umieszczone w odpowiednich folderach, tak jak w zalecanym rozkładzie plików opisanym poniżej. Jeżeli używasz edytora tekstu typu Sublime Text, dobrym pomysłem jest otworzenie w nim tego głównego folderu.

# Zalecany rozkład plików

```
nazwa_zadania             - głowny folder paczki
├── doc/tag.pdf           - treść (najlepiej też z plikiem tag.tex, kod źródłowy treści w latexie)
├── in/tag*.in            - testy (pliki wejściowe)
├── out/tag*.out          - testy (pliki wyjściowe)
├── prog/tag*.cpp         - wzorcówki / rozwiązania (też niekoniecznie poprawne)
├── check/checker.cpp     - checkerka (zazwyczaj niepotrzebna)
├── utils/                - miejsce na pomocnicze programy/skrypty, typu generatorka (ignorowany folder)
├── Simfile               - plik konfiguracyjny dla sim'a
└── Sipfile               - plik konfiguracyjny dla sip'a
```

# Treść

Jeżeli posiadasz już plik pdf z treścią zadania, umieść go w folderze doc/. Jeżeli nie, sip kompiluje kod źródłowy latexa komendą `sip doc`.
TODO: tutorial do pisania treści

# Wzorcówki / rozwiązania

W folderze prog/ umieść wszystkie rozwiązania do zadania. Bardzo zalecane jest napisanie rozwiązań wzorcowych, rozwiązań wolnych (brutów) i rozwiązań dających niepoprawne odpowiedzi. Sip akceptuje rozwiązania napisane w C, C++ (z rozszerzeniem .cpp, lub mniej znanymi .cc, .cxx) oraz w Pascalu (z rozszerzeniem .pas). Sip automatycznie traktuje rozwiązanie z najkrótszą nazwą jako rozwiązanie wzorcowe. W paczkach zadań Olimpiady Informatycznej można spotkać się ze zwyczajem, by rozwiązania zapisywać pod nazwami tag#.cpp, tags#.cpp, tagb#.cpp oznaczające kolejno rozwiązania poprawne, wolne i dające niepoprawne odpowiedzi (znak # oznacza dowolną liczbę), jednak sim nie wymaga takiego nazewnictwa plików. Przy dużej ilości rozwiązań rzeczywiście taki sposób jest sensowny, jednak zazwyczaj wystarczy nazywać pliki z krótkim opisem, np. tag_brute.cpp. 

Rozwiązania powinny wczytywać ze standardowego wejścia (np. cin) i wypisywać na standardowe wyjście (np. cout). Bardzo zalecane jest umieszczenie komentarza na samej górze każdego kodu z opisem rozwiązania (czy jest poprawne, czy nie - jak nie to dlaczego nie), opisem złożoności i autorem rozwiązania.


# Generatorka

By wygenerować testy, potrzebna jest generatorka. Program ma przyjmować argumenty i na standardowe wyjście (np. cout) wypisać test. Za pomocą argumentów określa się rozmiar testu, itp. Przykładowa generatorka:

``` cpp
#include <bits/stdc++.h>
using namespace std;
using L = long long;

mt19937_64 engine(chrono::system_clock::now().time_since_epoch().count());
// losowy long long z przedziału [a, b]: (więcej informacji na en.cppreference.com/w/cpp/numeric/random )
L rd(L a, L b) {
	return uniform_int_distribution<L>(a, b)(engine);
}

int main(int argc, char* argv[]) {
	// argument 0 jest pomijany
	constexpr int ilosc_argumentow = 2;
	// upewnienie się, że została wpisana dobra ilość argumentów
	assert(argc == ilosc_argumentow + 1);

	// zamienianie z tablicy charów na liczbę (więcej informacji na en.cppreference.com/w/cpp/string/byte/atoi )
	int max_n = atoi(argv[1]),
	    max_val = atoi(argv[2]);

	int n = rd(1, max_n);
	cout << n << '\n';
	for(int i = 0; i < n; ++i) {
		cout << rd(-max_val, max_val);
		if(i != n - 1)
			cout << ' ';
	}
	cout << '\n';
}
```

By ją uruchomić, należy skompilować ją normalnie i odpalić `./generatorka 5 10` (czyli argumentami do generatorki są liczby 5, a potem 10).

# Generowanie testów

Ustal jakie będą grupy testów, ile będzie testów w danej grupie i jakie będą ich wielkości. Przydziałem punktów do każdej grupy testów zajmiemy się później. Każdy plik wejściowy/wyjściowy zazwyczaj jest nazywany w sposób tag1a, gdzie '1' to numer grupy, a 'a' to numer testu w grupie. Testy przykładowe mają mieć numer grupy 0. Użytkownik widzi status swojego kodu na testach przykładowych.

Sip potrafi automatycznie generować testy za pomocą pliku Sipfile. Przykładowy plik Sipfile:
```
# czas (sekund), po którym przerywa wzorcówkę podczas generowania testów:
default_time_limit: 15
static: [
	# Tu umieść testy zrobione ręcznie (testy przykładowe)
	# Syntax: <test-range>
	tag0a
]
gen: [
	# Tu umieść zasady do generowania testów
	# Syntax: <test-range> <generator> [generator arguments]
	tag1a-f utils/generatorka.cpp 5 10
	tag2a utils/generatorka.cpp 100 100000
	tag2b utils/generatorka.cpp 1000 10000
	tag3a-5d utils/generatorka.cpp 100000 1000000000
]
```

Po zapisaniu pliku Sipfile, komenda `sip gen` skompiluje generatorkę oraz wzorcówkę (skompilowane pliki będą gdzieś w utils/cache/), następnie wygeneruje wszystkie pliki *.in za pomocą generatorki, a potem wszystkie pliki *.out za pomocą wzorcówki.

# Plik konfiguracyjny Simfile

Wszelkie ustawienia dotyczące paczki znajdują się w pliku Simfile.

Przykładowy, pełny plik Simfile:
```
name: Nazwa Zadania
label: tag
statement: doc/tag.pdf
solutions: [prog/tag.cpp, prog/tags1.cpp, prog/tagb1.cpp]
memory_limit: 256
checker: check/checker.cpp
limits: [
	0 0.4

	1a 0.4
	1b 0.4
	
	2 2.0
]
scoring: [
	0 0
	1 25
	2 75
]
tests_files: [
	0 in/tag0.in out/0.out
	1a in/tag1a.in out/tag1a.out
	1b in/tag1b.in out/tag1b.out
	2 in/tagb.in out/tagb.out
]
```
* name: nazwa zadania
* label: tag zadania
* memory_limit: limit pamięci (w MiB)
* scoring: dla każdej grupy testów ilość przyznawanych punktów (domyślnie daje po równo dla każdej grupy testów, tak by się sumowało do 100, oraz przydziela 0 pkt dla testów z grupy 0)

Następnie ustawienia, które sim zazwyczaj generuje samemu:

* solutions: lista programów (domyślnie nie trzeba pisać tej linijki, pierwsze rozwiązanie na liście będzie rozwiązaniem wzorcowym, względem którego będą ustalone limity czasowe oraz pliki out)
* checker: ścieżka do checkerki (domyślnie nie trzeba)
* limits: dla każdego testu (bez prefixu nazwy testu) maksymalny limit czasu w sekundach (domyślnie sam generuje)
* test_files: dla każdego testu ścieżka do pliku wejściowego/wyjściowego (domyślnie sam generuje)

Ponieważ nazwa zadania, label oraz limit pamięci są jedynymi parametrami, które są obowiązkowe, można je uzupełnić komendą (zamiast edytorem tekstu): `sip name 'Nazwa Zadania'`, `sip label 'tag'`, `sip mem [liczba]`. Podobnie istnieją komendy: `sip main-sol [ścieżka do pliku]`, `sip statement [ścieżka do pliku]`, `sip save scoring`.

# Checkerka

Jeżeli trzeba tylko porównać identyczność plików wyjściowych (z dokładnością do spacji/białych znaków), nie trzeba pisać checkerki, ponieważ sim sam taką swoją domyślną wrzuci.
TODO reszta

# Końcowe kroki

By doczytać o komendach sip'a, wpisz `sip`. By wytestować programy (zobaczyć ile punktów dostają), wpisz `sip test prog` (lub `sip test [prefix]`, gdzie `prefix` to prefix nazw rozwiązań, które należy wytestować). By zakończyć tworzenie paczki (wrzucić na sima), wpisz `sip zip` (automatyczne czyszczenie pozostałości / skompilowanych programów i skompresowanie paczki).

By wrzucić paczkę na sima, wejdź w zakładkę "Problems", następnie kliknij "Add problem". Zazwyczaj nie trzeba zmieniać żadnych opcji, wystarczy wybrać plik .zip i kliknąć "Submit". Grzecznie poczekaj, aż paczka się wrzuci (czyli aż nie pojawi się nowe okienko). Jak się paczka wrzuci, przywita cię okienko z napisem "Job pending". Wtedy sim przetwarza paczkę. Po pewnym czasie status okienka się zmieni (trzeba odświeżyć stronę). Jeżeli nastąpił error, zostanie wypisany szczegółowy raport.
