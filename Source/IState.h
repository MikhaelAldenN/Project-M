#pragma once

/**
 * @file IState.h
 * @brief [Fondasi FSM] Poin 1 To-Do List GDD.
 * Ini adalah interface (class abstrak) untuk semua state dalam game.
 * Setiap state (PlayerState_Idle, EnemyState_Stagger, dll.) akan
 * mewarisi (inherit) class ini dan mengimplementasikan fungsinya.
 */
class IState
{
public:
    // Destruktor virtual wajib ada untuk base class
    // Ini memastikan OnExit() dan destruktor state turunan dipanggil dengan benar
    // saat kita menghapusnya melalui pointer IState.
    virtual ~IState() {}

    /**
     * @brief Dipanggil satu kali saat StateMachine memasuki state ini.
     * Gunakan untuk inisialisasi, memutar animasi, dll.
     */
    virtual void OnEnter() = 0;

    /**
     * @brief Dipanggil setiap frame selagi state ini aktif.
     * Tempatkan logika utama state (pengecekan input, update, dll.) di sini.
     * @param delta_time Waktu (dalam detik) sejak frame terakhir.
     */
    virtual void Update(float delta_time) = 0;

    /**
     * @brief Dipanggil satu kali saat StateMachine meninggalkan state ini.
     * Gunakan untuk bersih-bersih, menghentikan suara, dll.
     */
    virtual void OnExit() = 0;
};