using System;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using UnityEngine;
using TMPro;
using System.Text.RegularExpressions;

public class ChatManager : MonoBehaviour
{
    public TMP_InputField inputField;
    public Transform contentHolder;
    public GameObject textPrefab;

    [DllImport("Eklamul_muariful_kebir", CharSet = CharSet.Ansi)]
    private static extern void InitAgent();

    [DllImport("Eklamul_muariful_kebir", CharSet = CharSet.Ansi)]
    private static extern IntPtr AskToGeminiC(string prompt);

    [DllImport("Eklamul_muariful_kebir", CharSet = CharSet.Ansi)]
    private static extern void CloseAgent();

    private string ToAscii(string input)
    {
        if (string.IsNullOrEmpty(input)) return input;
        // Türkçe karakterleri temizleyerek C++ tarafýndaki ASCII beklentisini karþýlýyoruz
        string temp = input.Replace("ç", "c").Replace("Ç", "C")
                           .Replace("ð", "g").Replace("Ð", "G")
                           .Replace("ý", "i").Replace("Ý", "I")
                           .Replace("ö", "o").Replace("Ö", "O")
                           .Replace("þ", "s").Replace("Þ", "S")
                           .Replace("ü", "u").Replace("Ü", "U");
        return Regex.Replace(temp, @"[^\u0000-\u007F]+", string.Empty);
    }

    void Start()
    {
        try
        {
            InitAgent();
            Debug.Log("<color=green>C++ Motoru Baþlatýldý.</color>");
        }
        catch (Exception e)
        {
            Debug.LogError("DLL Baðlantý Hatasý: " + e.Message);
        }
    }

    public async void MesajGonder()
    {
        string userText = inputField.text;
        if (string.IsNullOrWhiteSpace(userText)) return;

        EkranaYaziEkle("Siz: " + userText);
        inputField.text = "";

        // Masaüstü yolunu al ve Gemini'ýn anlayacaðý formata çevir
        string desktopPath = Environment.GetFolderPath(Environment.SpecialFolder.Desktop).Replace("\\", "/");

        // Prompt'u daha kompakt hale getiriyoruz (Gereksiz boþluklar silindi)
        string finalPrompt = $@"System: 
            Your environment: Windows.
            Your Desktop Path: {desktopPath}
            You are a Software Engineer Agent. Use the following tools strictly via JSON ARRAY when requested:

            CAPABILITIES:
            1. {{""tool"":""WRITE"",""path"":""..."",""content"":""...""}} -> Create or overwrite files (.py, .cpp, .cs).
            2. {{""tool"":""RUN"",""path"":""...""}} -> Executes files based on extension:
            - .py: runs 'python'
            - .cpp: compiles with 'g++' and runs
            - .cs: compiles with 'csc' and runs
            3. {{""tool"":""OPEN"",""path"":""...""}} -> Opens file/folder in Explorer.
            4. {{""tool"":""MKDIR"",""path"":""...""}} -> Creates directories.

            IMPORTANT: Respond with a JSON ARRAY if tools are needed. Otherwise, respond with plain text.

            User: {userText}";

        EkranaYaziEkle("Gemini: Düþünüyor...");

        try
        {
            string aiResponse = await Task.Run(() =>
            {
                // C++ tarafýna ASCII güvenli string gönderiyoruz
                IntPtr responsePtr = AskToGeminiC(ToAscii(finalPrompt));
                if (responsePtr == IntPtr.Zero) return "Hata: C++ tarafýnda boþ yanýt oluþtu.";

                // C++'dan gelen UTF-8 yanýtý C# string'ine çeviriyoruz
                return Marshal.PtrToStringAnsi(responsePtr); // C++ tarafý const char* dönüyorsa Ansi/UTF8 uygun
            });

            EkranaYaziEkle("Gemini: " + aiResponse);
        }
        catch (Exception ex)
        {
            EkranaYaziEkle("Hata: " + ex.Message);
            Debug.LogError("Ýþlem Hatasý: " + ex.Message);
        }
    }

    void EkranaYaziEkle(string mesaj)
    {
        GameObject yeniMesaj = Instantiate(textPrefab, contentHolder);
        yeniMesaj.GetComponent<TextMeshProUGUI>().text = mesaj;
        yeniMesaj.SetActive(true);
        Canvas.ForceUpdateCanvases();
        // Scroll'u aþaðý kaydýrmak için gerekirse ek mantýk buraya konabilir
    }

    void OnApplicationQuit()
    {
        CloseAgent();
    }
}