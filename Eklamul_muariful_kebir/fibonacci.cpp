from elevenlabs.client import ElevenLabs

client = ElevenLabs(api_key = "AQ.Ab8RN6LSROjsy-UF4JstwBQw-VWQ-iqDgAJbT6YV2ylbJrpWhw")

# Mevcut tüm sesleri listele
voices = client.voices.get_all()

for voice in voices.voices:
print(f"Ses Adý: {voice.name}, Ses ID: {voice.voice_id}")