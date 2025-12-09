import argparse
import glob
import multiprocessing
import os
import shutil
import time
import zipfile
import itertools
import logging
import sys
import re
import datetime
import requests
import subprocess
from subprocess import Popen, PIPE, STDOUT, run
from pathlib import Path

logging.basicConfig(
    format='%(asctime)s,%(msecs)03d %(levelname)-8s [%(filename)s:%(funcName)s:%(lineno)d] - %(message)s',
    datefmt='%Y-%m-%d:%H:%M:%S',
    level=logging.DEBUG)

pkg = 'com.bilibili.AzurLane'
pkg_version = '0'
mod_version = '0'
rootdir = os.getcwd()
#skip = False
#quick_rebuild = False


def is_windows() -> bool:
    return os.name in ['nt']



def mkcd(d):
    if not os.path.isdir(d):
        os.mkdir(d)
    os.chdir(d)



def executable_path(e, absolute=True):
    e = os.path.join(rootdir if absolute else '', 'bin', e)
    if is_windows():
        e += '.exe'
    return e


def bbox(cmd):
    # peak laziness
    busybox = os.path.join(rootdir, 'bin', 'busybox')
    if is_windows():
        busybox += '.exe'

    logging.info(f'{busybox} {cmd}')
    os.system(f'{busybox} {cmd}')


def get_version():
    global pkg_version
    logging.info('getting version from apk file in current directory')

    # prefer exact match
    candidates = sorted(glob.glob(f'{pkg}*.apk') + glob.glob('*.apk'))
    if not candidates:
        logging.warning('no apk found to extract version; falling back to timestamp')
        pkg_version = datetime.datetime.utcnow().strftime('%Y%m%d%H%M%S')
        return

    apk = candidates[0]
    logging.info(f'Found apk for version extraction: {apk}')

    # try using aapt if available
    try:
        proc = run(['aapt', 'dump', 'badging', apk], stdout=PIPE, stderr=STDOUT, text=True)
        if proc.returncode == 0:
            m = re.search(r"versionName='([^']+)'", proc.stdout)
            if m:
                pkg_version = m.group(1)
                logging.info(f'Extracted versionName via aapt: {pkg_version}')
                return
    except FileNotFoundError:
        logging.debug('aapt not found')

    # as last resort, use file mtime
    try:
        mtime = os.path.getmtime(apk)
        pkg_version = datetime.datetime.utcfromtimestamp(mtime).strftime('%Y%m%d%H%M%S')
        logging.warning(f'Using timestamp fallback version: {pkg_version}')
    except Exception:
        pkg_version = datetime.datetime.utcnow().strftime('%Y%m%d%H%M%S')
        logging.warning(f'Using UTC-now fallback version: {pkg_version}')

def download_jmbq_perseus_lib():
    global mod_version
    repo_owner = "JMBQ"
    repo_name = "azurlane"
    asset_pattern = "MOD_MENU_"
    extract_dir = Path("JMBQ-PerseusLib")
    suffix_to_cmd = {".rar": ["rar", "x", "-o+"],".zip": ["unzip", "-d"],".7z": ["7zz", "x", "-o"]}

    try:
        api_url = f"https://api.github.com/repos/{repo_owner}/{repo_name}/releases/latest"
        response = requests.get(api_url, timeout=10)
        response.raise_for_status()
        release_data = response.json()
        target_asset = None
        for asset in release_data.get("assets", []):
            if asset_pattern in asset["name"] and asset["name"].endswith((".rar", ".zip", ".7z")):
                target_asset = asset
                break   
        if not target_asset:
            raise ValueError("cant find MOD_MENU")

        download_url = target_asset["browser_download_url"]
        temp_file = Path(f"temp_{target_asset['name']}")
        
        with requests.get(download_url, stream=True, timeout=30) as r:
            r.raise_for_status()
            with open(temp_file, "wb") as f:
                for chunk in r.iter_content(chunk_size=8192):
                    f.write(chunk)

        asset_name = target_asset["name"]
        version_match = re.search(r"MOD_MENU_([\d\.]+)\.(rar|zip|7z)", asset_name)
        mod_version = version_match.group(1)
        print(f"mod_version: {mod_version}")

        extract_dir.mkdir(exist_ok=True)
        suffix = Path(asset_name).suffix
        cmd = suffix_to_cmd[suffix]

        if suffix == ".zip":
            cmd += [str(extract_dir), str(temp_file)]
        elif suffix == ".7z":
            cmd += [str(extract_dir), str(temp_file)]
            subprocess.run(
                ["mv", f"{str(extract_dir)}/*/*", str(extract_dir)],
                capture_output=True,
                text=True
            )
        else:
            cmd += [str(temp_file), str(extract_dir)]

        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True
        )

        if result.returncode != 0:
            raise RuntimeError(f"{result.stderr}")
                
    finally:
        logging.info('download complited.')
        #if 'temp_file' in locals() and temp_file.exists():
        #    temp_file.unlink()


'''
def build_perseus_lib(do_clean=False):
    logging.info(f'{"cleaning" if do_clean else "building"} perseus libs')

    os.chdir('JMBQ-PerseusLib')

    cmd = [f'ndk-build{".cmd" if is_windows() else ""}',
           'NDK_PROJECT_PATH=./src',
           'NDK_APPLICATION_MK=./src/Application.mk',
           'APP_BUILD_SCRIPT=./src/Android.mk',
           'APP_PLATFORM=android-21',
           f'-j{multiprocessing.cpu_count()}'] + (['clean'] if do_clean else [])

    ndk_proc = run(cmd, capture_output=True, text=True)

    output = ndk_proc.stdout
    return_code = ndk_proc.returncode

    if return_code != 0:
        logging.error('ndk-build failed')

        print("======== ndk-build stdout ========", file=sys.stderr)
        print(output, file=sys.stderr)
        print("======== ndk-build stderr ========", file=sys.stderr)
        print(ndk_proc.stderr, file=sys.stderr)
        exit(1)

    os.chdir('..')
'''

def extract_from_packages():
    '''
    if skip and os.path.isfile(f'{pkg}.apk'):
        logging.info(f'{pkg}.apk already exists, skipping')
        return
    '''

    logging.info('searching for package archives in packages/')
    # Prefer archives that start with the pkg name
    candidates = sorted(glob.glob(os.path.join(rootdir, 'packages', f'{pkg}*')))
    if not candidates:
        # fallback to any zip / 7z in packages
        candidates = sorted(glob.glob(os.path.join(rootdir, 'packages', '*.zip')) +
                            glob.glob(os.path.join(rootdir, 'packages', '*.7z')) +
                            glob.glob(os.path.join(rootdir, 'packages', '*part*')))

    if not candidates:
        logging.error('No package archives found in packages/; expected something like com.bilibili.AzurLane.zip or split archives.')
        exit(1)

    archive = candidates[0]
    logging.info(f'Using archive: {archive}')

    # Try to use bundled 7zz first
    sevenz = executable_path('7zz')
    if os.path.isfile(sevenz):
        logging.info(f'extracting with {sevenz}')
        proc = run([sevenz, 'x', '-y', archive], stdout=PIPE, stderr=STDOUT, text=True)
        if proc.returncode != 0:
            logging.error('7zz extraction failed')
            print(proc.stdout, file=sys.stderr)
            exit(1)
    else:
        # fallback to unzip
        logging.info('7zz not found in bin/, falling back to system unzip')
        proc = run(['unzip', '-o', archive, '-d', '.'], stdout=PIPE, stderr=STDOUT, text=True)
        if proc.returncode != 0:
            logging.error('unzip extraction failed')
            print(proc.stdout, file=sys.stderr)
            exit(1)

    if not os.path.isfile(f'{pkg}.apk'):
        logging.error(f'After extraction could not find {pkg}.apk in apk_build/ (extracted files: {os.listdir(".")})')
        exit(1)


def decompile_apk():
    '''
    if skip and os.path.isdir(pkg):
        logging.info(f'{pkg}.apk is already decompiled, skipping')
        return
    '''
        
    logging.info(f'decompiling {pkg}.apk')
    if os.path.isdir(pkg):
        shutil.rmtree(pkg)
    os.system(f'java -jar {os.path.join(rootdir, "bin", "apktool.jar")} -q -f d {pkg}.apk')


def copy_perseus_libs():
    logging.info('copying Perseus libs')
    bbox(f'sh -c "cp -r ../JMBQ-PerseusLib/* {pkg}/"')


def patch():
    logging.info('patching decompiled sources')
    bbox(f"sh {os.path.join(rootdir, 'scripts', 'patch.sh')}")


def rebuild():
    newpkg = f'{pkg}_{pkg_version}-JMBQ_{mod_version}-patched.apk'
    ''' 
    newzip = newpkg + '.zip'
    if quick_rebuild and os.path.isfile(newpkg):
        logging.info(f'rebuiling {pkg}.apk quickly')

        shutil.move(newpkg, newzip)

        libs = [f'{pkg}/lib/{arch}/libPerseus.so' for arch in ['arm64-v8a', 'x86_64', 'x86']]
        libs_renamed = [lib.removeprefix(f'{pkg}/') for lib in libs]

        proc1 = run([executable_path('7zz'), '-y', 'd', newzip] + libs_renamed, stdout=PIPE)
        logging.info(f'deleting libs in archive, ret={proc1.returncode}')

        proc2 = run([executable_path('7zz'), '-y', 'a', newzip] + libs, stdout=PIPE)
        logging.info(f'adding libs to archive, ret={proc2.returncode}')

        proc3 = run([executable_path('7zz'), '-y', 'rn', newzip] + list(itertools.chain.from_iterable(zip(libs, libs_renamed))), stdout=PIPE)
        logging.info(f'renaming libs in archive, ret={proc3.returncode}')

        shutil.move(newzip, newpkg)

        return
    '''

    logging.info(f'rebuilding {pkg}.apk with apktool')
    os.system(f'java -jar {os.path.join(rootdir, "bin", "apktool.jar")} -q -f b {pkg} -o {newpkg}')


def sign_apk():
    f = f'{pkg}_{pkg_version}-JMBQ_{mod_version}-patched.apk'
    shutil.move(f, f + '.unsigned')

    logging.info('zipaligning apk')
    zipalign = f'zipalign{".exe" if is_windows() else ""}'
    os.system(f'{zipalign} -pf 4 {f}.unsigned {f}')
    os.remove(f'{f}.unsigned')

    logging.info('signing apk')
    apksigner = f'apksigner{".bat" if is_windows() else ""}'
    key = os.path.join(rootdir, 'signing', 'testkey.pk8')
    cert = os.path.join(rootdir, 'signing', 'testkey.x509.pem')
    os.system(f'{apksigner} sign --key {key} --cert {cert} {f}')

    os.remove(f'{f}.idsig')


'''
def compress_libs():
    logging.info('compressing Perseus libs into apk_build')
    libs_dir = os.path.join(rootdir, 'PerseusLib', 'src', 'libs')
    out_zip = os.path.join(os.getcwd(), 'perseus_libs.zip')

    if not os.path.isdir(libs_dir):
        logging.warning(f'Perseus libs dir not found: {libs_dir}, skipping compress')
        return

    with zipfile.ZipFile(out_zip, 'w', zipfile.ZIP_DEFLATED) as zf:
        for root, _, files in os.walk(libs_dir):
            for f in files:
                full = os.path.join(root, f)
                arcname = os.path.relpath(full, libs_dir)
                zf.write(full, arcname)

    logging.info(f'Written libs archive: {out_zip}')
'''

def main():
    '''    
    global skip, quick_rebuild

    parser = argparse.ArgumentParser(
        prog='perseus apk builder',
        description='builds apk for you (this is the default behaviour if called with no arguments)')

    parser.add_argument('--skip', 
                        help='skip decompile and extracting if possible', 
                        default=True,
                        action=argparse.BooleanOptionalAction)
    parser.add_argument('--quick-rebuild',
                        help='rebuild apk by replacing libs in the apk instead of using apktool (saves 40s)',
                        default=True,
                        action=argparse.BooleanOptionalAction)
    args = parser.parse_args()

    skip = args.skip
    quick_rebuild = args.quick_rebuild
    '''

    start = time.time()
    download_jmbq_perseus_lib()
    #build_perseus_lib()
    mkcd('apk_build')
    extract_from_packages()
    get_version()
    decompile_apk()
    copy_perseus_libs()
    patch()
    rebuild()
    sign_apk()
    #compress_libs()
    end = time.time()

    logging.info(f"built apk in {os.path.join(rootdir, 'apk_build', f'{pkg}_{pkg_version}-JMBQ_{mod_version}-patched.apk')}")
    logging.info(f"done in {round(end - start, 2)} seconds")


if __name__ == '__main__':
    main()
