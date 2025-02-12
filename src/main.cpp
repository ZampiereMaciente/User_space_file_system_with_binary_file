#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <cstring>

struct Superblock {
    int total_blocks;
    int free_blocks;
    int block_size;
    int data_offset;
};

struct Inode {
    std::vector<int> blocks;
    int size;
    bool is_used;
};

struct Dentry {
    std::string name;
    int inode_number;
};

class FileSystem {
private:
    Superblock superblock;
    std::unordered_map<int, Inode> inodes;
    std::unordered_map<std::string, Dentry> dentries;
    std::fstream fs_file;
    std::string fs_filename;
    int next_inode;

    // Metodos de persistencia
    void saveMetadata() {
        fs_file.seekp(0);
        
        // Salva superblock
        fs_file.write(reinterpret_cast<const char*>(&superblock), sizeof(Superblock));
        
        // Salva dentries
        size_t dentry_count = dentries.size();
        fs_file.write(reinterpret_cast<const char*>(&dentry_count), sizeof(size_t));
        for (const auto& [name, dentry] : dentries) {
            size_t name_size = name.size();
            fs_file.write(reinterpret_cast<const char*>(&name_size), sizeof(size_t));
            fs_file.write(name.c_str(), name_size);
            fs_file.write(reinterpret_cast<const char*>(&dentry.inode_number), sizeof(int));
        }
        
        // Salva inodes
        size_t inode_count = inodes.size();
        fs_file.write(reinterpret_cast<const char*>(&inode_count), sizeof(size_t));
        for (const auto& [num, inode] : inodes) {
            fs_file.write(reinterpret_cast<const char*>(&num), sizeof(int));
            fs_file.write(reinterpret_cast<const char*>(&inode.size), sizeof(int));
            size_t block_count = inode.blocks.size();
            fs_file.write(reinterpret_cast<const char*>(&block_count), sizeof(size_t));
            fs_file.write(reinterpret_cast<const char*>(inode.blocks.data()), block_count * sizeof(int));
        }
    }

    void loadMetadata() {
        fs_file.seekg(0);
        
        // Carrega superblock
        fs_file.read(reinterpret_cast<char*>(&superblock), sizeof(Superblock));
        
        // Carrega dentries
        size_t dentry_count;
        fs_file.read(reinterpret_cast<char*>(&dentry_count), sizeof(size_t));
        for (size_t i = 0; i < dentry_count; ++i) {
            size_t name_size;
            fs_file.read(reinterpret_cast<char*>(&name_size), sizeof(size_t));
            std::string name(name_size, '\0');
            fs_file.read(name.data(), name_size);
            
            int inode_number;
            fs_file.read(reinterpret_cast<char*>(&inode_number), sizeof(int));
            
            dentries[name] = {name, inode_number};
        }
        
        // Carrega inodes
        size_t inode_count;
        fs_file.read(reinterpret_cast<char*>(&inode_count), sizeof(size_t));
        for (size_t i = 0; i < inode_count; ++i) {
            int inode_number;
            Inode inode;
            fs_file.read(reinterpret_cast<char*>(&inode_number), sizeof(int));
            fs_file.read(reinterpret_cast<char*>(&inode.size), sizeof(int));
            
            size_t block_count;
            fs_file.read(reinterpret_cast<char*>(&block_count), sizeof(size_t));
            inode.blocks.resize(block_count);
            fs_file.read(reinterpret_cast<char*>(inode.blocks.data()), block_count * sizeof(int));
            
            inodes[inode_number] = inode;
        }
        
        // Encontra proximo inode disponivel
        next_inode = 1;
        for (const auto& [num, _] : inodes) {
            if (num >= next_inode) next_inode = num + 1;
        }
    }

    int allocateBlock() {
        if (superblock.free_blocks <= 0) return -1;
        return superblock.total_blocks - superblock.free_blocks--;
    }

public:
    FileSystem(const std::string& filename, int block_size, int total_blocks)
        : fs_filename(filename) {
        
        struct stat buffer;
        bool file_exists = (stat(filename.c_str(), &buffer) == 0);
        
        // Abri ou criar arquivo
        fs_file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
        if (!fs_file) {
            // Cria novo sistema de arquivos
            fs_file.open(filename, std::ios::out | std::ios::binary);
            fs_file.close();
            fs_file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
            
            superblock.total_blocks = total_blocks;
            superblock.free_blocks = total_blocks;
            superblock.block_size = block_size;
            superblock.data_offset = sizeof(Superblock) + 1024; // Reserva 1KB para metadados
            
            // Formata arquivo
            std::vector<char> empty(superblock.data_offset + total_blocks * block_size, 0);
            fs_file.write(empty.data(), empty.size());
            fs_file.flush();
            
            next_inode = 1;
        } else {
            // Carrega sistema existente
            if (file_exists && buffer.st_size > 0) {
                loadMetadata();
            }
        }
    }

    ~FileSystem() {
        saveMetadata();
        fs_file.close();
    }

    void createFile(const std::string& name) {
        if (dentries.count(name)) {
            std::cout << "Erro: Arquivo já existe!\n";
            return;
        }
        
        Inode new_inode;
        new_inode.size = 0;
        new_inode.is_used = true;
        
        int block = allocateBlock();
        if (block == -1) {
            std::cout << "Erro: Sem espaço livre!\n";
            return;
        }
        
        new_inode.blocks.push_back(block);
        inodes[next_inode] = new_inode;
        dentries[name] = {name, next_inode};
        
        std::cout << "Arquivo criado: " << name << " (inode " << next_inode << ")\n";
        next_inode++;
    }

    void copyToFileSystem(const std::string& real_path) {
        std::ifstream src_file(real_path, std::ios::binary | std::ios::ate);
        if (!src_file) {
            std::cout << "Erro ao abrir arquivo fonte!\n";
            return;
        }
        
        std::string name = real_path.substr(real_path.find_last_of("/\\") + 1);
        if (dentries.count(name)) {
            std::cout << "Erro: Arquivo já existe no sistema!\n";
            return;
        }
        
        const size_t file_size = src_file.tellg();
        src_file.seekg(0);
        
        const int blocks_needed = (file_size + superblock.block_size - 1) / superblock.block_size;
        if (blocks_needed > superblock.free_blocks) {
            std::cout << "Erro: Espaço insuficiente (" << blocks_needed << " blocos necessários)\n";
            return;
        }
        
        Inode new_inode;
        new_inode.size = file_size;
        new_inode.is_used = true;
        
        for (int i = 0; i < blocks_needed; ++i) {
            int block = allocateBlock();
            if (block == -1) {
                std::cout << "Erro durante alocação de blocos!\n";
                return;
            }
            new_inode.blocks.push_back(block);
        }
        
        // Copia dados
        char* buffer = new char[superblock.block_size];
        for (size_t i = 0; i < new_inode.blocks.size(); ++i) {
            src_file.read(buffer, superblock.block_size);
            const size_t offset = superblock.data_offset + new_inode.blocks[i] * superblock.block_size;
            fs_file.seekp(offset);
            fs_file.write(buffer, src_file.gcount());
        }
        delete[] buffer;
        
        inodes[next_inode] = new_inode;
        dentries[name] = {name, next_inode};
        next_inode++;
        
        std::cout << "Arquivo copiado: " << name << " (" << file_size << " bytes)\n";
    }

    void moveToFileSystem(const std::string& real_path) {
        copyToFileSystem(real_path);
        if (std::remove(real_path.c_str()) != 0) {
            std::cout << "Aviso: Não foi possível remover o arquivo original!\n";
        }
    }

    void moveToRealSystem(const std::string& name) {
        if (!dentries.count(name)) {
            std::cout << "Erro: Arquivo não encontrado!\n";
            return;
        }
        
        const int inode_num = dentries[name].inode_number;
        const Inode& inode = inodes[inode_num];
        
        std::ofstream dst_file(name, std::ios::binary);
        if (!dst_file) {
            std::cout << "Erro ao criar arquivo destino!\n";
            return;
        }
        
        // Le dados
        char* buffer = new char[superblock.block_size];
        for (int block : inode.blocks) {
            const size_t offset = superblock.data_offset + block * superblock.block_size;
            fs_file.seekg(offset);
            fs_file.read(buffer, superblock.block_size);
            dst_file.write(buffer, fs_file.gcount());
        }
        delete[] buffer;
        
        // Libera blocos
        superblock.free_blocks += inode.blocks.size();
        inodes.erase(inode_num);
        dentries.erase(name);
        
        std::cout << "Arquivo movido para sistema real: " << name << "\n";
    }

    void deleteFile(const std::string& name) {
        if (!dentries.count(name)) {
            std::cout << "Erro: Arquivo não encontrado!\n";
            return;
        }
        
        const int inode_num = dentries[name].inode_number;
        const Inode& inode = inodes[inode_num];
        
        superblock.free_blocks += inode.blocks.size();
        inodes.erase(inode_num);
        dentries.erase(name);
        
        std::cout << "Arquivo excluído: " << name << "\n";
    }

    void listFiles() {
    std::cout << "\nSistema de Arquivos: " << fs_filename
              << "\nBlocos Livres: " << superblock.free_blocks << "/" << superblock.total_blocks
              << "\nTamanho do Bloco: " << superblock.block_size << " bytes\n\n";
                  
    for (const auto& [name, dentry] : dentries) {
        const Inode& inode = inodes[dentry.inode_number];
        std::cout << "Nome: " << name << std::string(25 - name.length(), ' ')
                  << " | Inode: " << dentry.inode_number
                  << " | Tamanho: " << inode.size << " bytes"
                  << " | Blocos: " << inode.blocks.size() << "\n";
    }
    std::cout << "\n";
    }
};

int main() {
    FileSystem fs("arquivo_binario.bin", 4096, 100000);  // Sistema com blocos de 4KB e 10000 blocos

    while (true) {
        std::cout << "1. Lista arquivos\n"
                  << "2. Cria arquivo vazio\n"
                  << "3. Copia arquivo do sistema real\n"
                  << "4. Move arquivo do sistema real\n"
                  << "5. Move arquivo para sistema real\n"
                  << "6. Exclui arquivo\n"
                  << "7. Sair\n"
                  << "Opção: ";
        
        int choice;
        std::cin >> choice;
        std::cin.ignore();

        std::string path;
        switch (choice) {
            case 1:
                fs.listFiles();
                break;
            case 2:
                std::cout << "Nome do arquivo: ";
                std::getline(std::cin, path);
                fs.createFile(path);
                break;
            case 3:
                std::cout << "Caminho completo ou o nome do arquivo: ";
                std::getline(std::cin, path);
                fs.copyToFileSystem(path);
                break;
            case 4:
                std::cout << "Caminho completo ou o nome do arquivo: ";
                std::getline(std::cin, path);
                fs.moveToFileSystem(path);
                break;
            case 5:
                std::cout << "Nome do arquivo: ";
                std::getline(std::cin, path);
                fs.moveToRealSystem(path);
                break;
            case 6:
                std::cout << "Nome do arquivo: ";
                std::getline(std::cin, path);
                fs.deleteFile(path);
                break;
            case 7:
                return 0;
            default:
                std::cout << "Opção invalida!\n";
        }
    }
}