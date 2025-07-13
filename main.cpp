#include <iostream>
#include <windows.h> // handle, OpenProcess, TerminateProcess, CloseHandle, SetConsoleOutputCP, CP_UTF8
#include <tlhelp32.h> // CreateToolhelp32Snapshot, Process32First, Process32Next, PROCESSENTRY32W...
#include <iomanip> // Para setw
#include <psapi.h> // Para GetProcessMemoryInfo
#include <locale> // Para std::wstring_convert
#include <codecvt> // Para std::codecvt_utf8 

struct Processo {
    DWORD pid;
    std::string nome;
    SIZE_T memoria;
};

struct NoArvore {
    Processo proc;
    NoArvore* esquerda;
    NoArvore* direita;
};

void inserirNaArvore(NoArvore*& raiz, Processo p) {
    if (raiz == nullptr) 
    {
        raiz = new NoArvore{p, nullptr, nullptr};
        return;
    }
    
    if (p.memoria < raiz->proc.memoria) 
    {
        inserirNaArvore(raiz->esquerda, p);
    } 
    else if (p.memoria > raiz->proc.memoria) 
    {
        inserirNaArvore(raiz->direita, p);
    } 
    else 
    {
        // Se memória for igual, usar o PID para decidir o lado
        if (p.pid < raiz->proc.pid) {
            inserirNaArvore(raiz->esquerda, p);
        } else if (p.pid > raiz->proc.pid) {
            inserirNaArvore(raiz->direita, p);
        }
        // Se o PID também for igual, não insere (processo duplicado)
    }

}

void imprimirOrdemDecrescente(NoArvore* raiz) {
    if (raiz == nullptr) return;

    imprimirOrdemDecrescente(raiz->direita);

    std::cout << std::left
              << std::setw(8) << raiz->proc.pid
              << std::setw(35) << raiz->proc.nome
              << std::right << std::setw(12) << raiz->proc.memoria / 1024 << " KB" 
              << std::endl;

    imprimirOrdemDecrescente(raiz->esquerda);
}


HANDLE snapShotProcessos()
{
    // CreateToolhelp32Snapshot é uma função que cria um snapshot de todos os processos em execução no sistema
    // TH32CS_SNAPPROCESS é uma constante que indica que queremos um snapshot de processos
    // 0 indica que não estamos filtrando por PID específico, ou seja, queremos todos
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); 
    
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::cerr << "Erro ao criar snapshot de processos." << std::endl;
        return NULL;
    }
    
    return hSnapshot;
}

void encerrarProcesso(DWORD pid)
{
    // PROCESS_TERMINATE permite terminar o processo
    // FALSE indica que não estamos solicitando acesso de thread
    // pid é o ID do processo que queremos encerrar
    // OpenProcess retorna um handle para o processo, que pode ser usado para interagir com ele
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid); // Abre o processo com permissão para terminar
    if (hProcess == NULL) {
        std::cerr << "Erro ao abrir o processo com PID: " << pid << std::endl;
        return;
    }

    if (TerminateProcess(hProcess, 0) == FALSE) { // Tenta terminar o processo
        std::cerr << "Erro ao encerrar o processo com PID: " << pid << std::endl;
    } else {
        std::cout << "Processo com PID: " << pid << " encerrado com sucesso." << std::endl;
    }

    CloseHandle(hProcess);
}

SIZE_T memoryInfo(PROCESSENTRY32W& pe32)
{
    // PROCESS_QUERY_INFORMATION permite obter informações sobre o processo
    // PROCESS_VM_READ permite ler a memória do processo
    // FALSE indica que não estamos solicitando acesso de thread
    // pe32.th32ProcessID é o ID do processo que queremos abrir
    // OpenProcess retorna um handle para o processo, que pode ser usado para interagir com ele
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
    
    if (hProcess == NULL) {
        return 0;
    }

    PROCESS_MEMORY_COUNTERS pmc; // Estrutura para armazenar informações de memória do processo   

    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)) == TRUE) 
    {
        CloseHandle(hProcess);
        return pmc.WorkingSetSize;
    } 
    else 
    {
        std::cerr << "Erro ao obter informações de memória do processo: " << pe32.szExeFile << std::endl;
        CloseHandle(hProcess);
        return 0;
    }    
}

void listaProcessos()
{
    NoArvore* raiz = nullptr; // Inicializa a raiz da árvore como nula

    HANDLE hSnapshot = snapShotProcessos(); // Cria um snapshot de processos

    // PROCESSENTRY32 é uma estrutura que contém informações sobre um processo
    // dwSize deve ser definido para o tamanho da estrutura antes de usá-la
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    // Process32First obtém informações sobre o primeiro processo no snapshot
    if (Process32FirstW(hSnapshot, &pe32) == FALSE) {
        std::cerr << "Erro ao obter o primeiro processo." << std::endl;
        CloseHandle(hSnapshot);
        return;
    }

    system("cls"); // Limpa a tela do console

    std::cout << std::left 
              << std::setw(8)  << "PID"
              << std::setw(35) << "Nome do Processo"
              << std::right << std::setw(12) << "Memoria"
              << std::endl;
    std::cout << std::string(55, '-') << std::endl;


    do {
        SIZE_T hmem = memoryInfo(pe32); // Chama a função memoryInfo para obter informações de memória do processo

        if(hmem == 0) {
            continue; // Se não foi possível obter informações de memória, pula para o próximo processo
        }

        Processo proc;
        proc.pid = pe32.th32ProcessID; // Obtém o PID do processo

        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter; // Cria um conversor de wstring para string (UTF-8)
        proc.nome = converter.to_bytes(pe32.szExeFile); // Converte o nome do processo de wstring para string

        proc.memoria = hmem; // Atribui o tamanho da memória obtida

        inserirNaArvore(raiz, proc); // Insere o processo na árvore
                    
    // pe32 é passado por referência para Process32Next, que atualiza suas informações
    } while (Process32NextW(hSnapshot, &pe32)); // Process32Next obtém informações sobre o próximo processo no snapshot
    // condição de parada: Process32Next retorna FALSE quando não há mais processos para listar

    if (raiz == nullptr) {
        std::cout << "Nenhum processo encontrado." << std::endl;
    } else {
        imprimirOrdemDecrescente(raiz); // Imprime os processos em ordem decrescente de uso de memória
    }

    CloseHandle(hSnapshot);
}

int main()
{
    SetConsoleOutputCP(CP_UTF8); // Corrige acentuação no console

    int option;
    DWORD pid;

    while(true)
    {
        listaProcessos();
        std::cout << "\nPressione 1 para atualizar a lista de processos, 2 para encerrar um processo ou 0 para sair: ";
        std::cin >> option;

        switch (option)
        {
            case 1:
                continue; // Atualiza a lista de processos
            case 2:
                std::cout << "Digite o PID do processo que deseja encerrar: ";
                std::cin >> pid;
                encerrarProcesso(pid);
                break;
            case 0:
                return 0; // Sai do programa
            default:
                std::cout << "Opção inválida. Tente novamente." << std::endl;
        }

        Sleep(3000); // Pausa por 1 segundo antes de atualizar a lista novamente
        
    }
}